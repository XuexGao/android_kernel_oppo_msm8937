/***************************************************
 * File:touch_interfaces.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             Touch interface
 * Version:1.0:
 * Date created:2016/09/02
 * Author: Tong.han@Bsp.Driver
 * TAG: BSP.TP.Init
 * *
 * -------------- Revision History: -----------------
 *  <author >  <data>  <version>  <desc>
 ***************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include "touch_interfaces.h"

static bool register_is_16bit = 0;

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM  // mtk

static u8 *gprDMABuf_va = NULL;
static dma_addr_t gprDMABuf_pa = 0;
static u8 *gpwDMABuf_va = NULL;
static dma_addr_t gpwDMABuf_pa = 0;

int touch_i2c_read_block(struct i2c_client* client, u16 addr, unsigned short length, unsigned char *data)
{
    int retval;
    s32 retry = 0;

    unsigned char buffer[2] = {(addr >> 8) & 0xff, addr & 0xff};    
    struct i2c_msg msg[2];

    msg[0].addr = (client->addr & I2C_MASK_FLAG);
    msg[0].flags = 0;
    msg[0].buf = buffer;
    msg[0].timing = I2C_MASTER_CLOCK;

    if (!register_is_16bit)  // if register is 8bit
    {
        msg[0].len = 1;
        msg[0].buf[0] = buffer[1];
    }
    else
    {
        msg[0].len = 2;            
        msg[0].buf[0] = buffer[0];
        msg[0].buf[1] = buffer[1];
    }

    msg[1].addr = client->addr;
    msg[1].ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
    msg[1].flags = I2C_M_RD;
    msg[1].buf = (u8 *)gprDMABuf_pa;   
    msg[1].len = length;    
    msg[1].timing = I2C_MASTER_CLOCK;

    if (data == NULL) {
        return -1;
    }
    for (retry = 0; retry < MAX_I2C_RETRY_TIME; retry++) {
        if (i2c_transfer(client->adapter, msg, 2) == 2) {
            retval = length;
            break;
        }
        msleep(20);
    }
    if (retry == MAX_I2C_RETRY_TIME) {
        dev_err(&client->dev, "%s: I2C read over retry limit\n", __func__);
        retval = -EIO;
    } 
    memcpy(data, gprDMABuf_va, length);
    return retval;
}

int touch_i2c_write_block(struct i2c_client* client, u16 addr, unsigned short length, unsigned char const *data)
{
    int retval;
    s32 retry = 0;
    u8 *wr_buf = gpwDMABuf_va;    
    struct i2c_msg msg[1];

    msg[0].addr = (client->addr & I2C_MASK_FLAG);
    msg[0].ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
    msg[0].flags = 0;
    msg[0].buf = (u8 *)gpwDMABuf_pa;
    msg[0].timing = I2C_MASTER_CLOCK;

    if (!register_is_16bit)  // if register is 8bit
    {
        msg[0].len = length + 1;
        msg[0].buf[0] = addr & 0xff;

        memcpy(wr_buf + 1, data, length);        
    }
    else
    {
        msg[0].len = length + 2;     
        msg[0].buf[0] = (addr >> 8) & 0xff;
        msg[0].buf[1] = addr & 0xff;

        memcpy(wr_buf + 2, data, length);        
    }

    for (retry = 0; retry < MAX_I2C_RETRY_TIME; retry++) {
        if (i2c_transfer(client->adapter, msg, 1) == 1) {
            retval = length;
            break;
        }
        msleep(20);
    }
    if (retry == MAX_I2C_RETRY_TIME) {
        dev_err(&client->dev, "%s: I2C write over retry limit\n", __func__);        
        retval = -EIO;
    } 
    return retval;
}

#else/*else of CONFIG_TOUCHPANEL_MTK_PLATFORM*/

/**
 * touch_i2c_read_block - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to write
 * @length: data size we want to send
 * @data: data we want to send
 *
 * Actully, This function call i2c_transfer for IIC transfer, 
 * Returning transfer length(transfer success) or most likely negative errno(transfer error)
 */
int touch_i2c_read_block(struct i2c_client* client, u16 addr, unsigned short length, unsigned char *data)
{
    int retval;
    unsigned char retry;
    /*
     * 传给 i2c_transfer 的缓冲区必须是**直接映射区**的内存。
     *
     * 原因：dma_map_single() 内部走 virt_to_page()，它只对线性映射地址成立。
     * msm i2c 驱动（i2c_msm_qup_choose_mode）在传输量 >= FIFO 大小时会切到
     * DMA 模式，并对 msg->buf 调 dma_map_single()；此时若 buf 是栈上数组，
     * virt_to_page() 会算出垃圾 page，最终在
     *     __swiotlb_map_page -> __clean_dcache_area_poc
     * 里访问一个没有建立页表的地址，直接 Oops。
     *
     * 这里原先把寄存器地址放在栈上的 buffer[2]、把数据直接写进调用方的
     * data —— 读 186 字节面板配置这种大传输会同时踩到这两个坑。
     * 改为统一用 kmalloc 申请转发缓冲，读完再拷回 data。
     */
    unsigned char *stage;
    struct i2c_msg msg[2];

    stage = kmalloc(length + 2, GFP_KERNEL);
    if (!stage)
        return -ENOMEM;

    /* stage[0..1] 放寄存器地址，stage[2..] 放读回的数据 */
    stage[0] = (addr >> 8) & 0xff;
    stage[1] = addr & 0xff;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = stage;

    if (!register_is_16bit)  // if register is 8bit
    {
        msg[0].len = 1;
        msg[0].buf[0] = stage[1];
    }
    else
    {
        msg[0].len = 2;
        msg[0].buf[0] = stage[0];
        msg[0].buf[1] = stage[1];
    }

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = length;
    msg[1].buf = stage + 2;

    for (retry = 0; retry < MAX_I2C_RETRY_TIME; retry++) {
        if (i2c_transfer(client->adapter, msg, 2) == 2) {
            retval = length;
            break;
        }
        msleep(20);
    }
    if (retry == MAX_I2C_RETRY_TIME) {
        dev_err(&client->dev, "%s: I2C read over retry limit\n", __func__);
        retval = -EIO;
    } else {
        memcpy(data, stage + 2, length);
    }

    kfree(stage);
    return retval;
}

/**
 * touch_i2c_write_block - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to write
 * @length: data size we want to send
 * @data: data we want to send
 *
 * Actully, This function call i2c_transfer for IIC transfer, 
 * Returning transfer length(transfer success) or most likely negative errno(transfer error)
 */
int touch_i2c_write_block(struct i2c_client* client, u16 addr, unsigned short length, unsigned char const *data)
{
    int retval;
    unsigned char retry;
    /*
     * 原来是 unsigned char buffer[length + 2]; —— 一个栈上的 VLA，
     * 当 length 达到面板配置的长度（Goodix GT5688 是 186~240 字节，
     * 见 gt1x_generic_oppo.h 的 GTP_CONFIG_MIN/MAX_LENGTH）时，
     * I2C 会切到 DMA 模式并把栈地址交给 dma_map_single()，
     * 于是 virt_to_page() 算出垃圾 page，在 __clean_dcache_area_poc
     * 里踩到未映射地址：
     *     Unable to handle kernel paging request at ffffffc01004b300
     *     pc : __clean_dcache_area_poc / lr : __swiotlb_map_page
     *     gt1x_send_cfg -> touch_i2c_write_block -> i2c_transfer
     * 改成 kmalloc 申请，保证是线性映射内存。
     *
     * 对照：R9s 的 Synaptics 驱动每次只写几十字节（< FIFO），一直走
     * FIFO 模式不碰 DMA，所以同一个内核里 R9s 正常、A57 崩。
     */
    unsigned char *buffer;
    struct i2c_msg msg[1];

    buffer = kmalloc(length + 2, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = buffer;

    if (!register_is_16bit)  // if register is 8bit
    {
        msg[0].len = length + 1;
        msg[0].buf[0] = addr & 0xff;

        memcpy(&buffer[1], &data[0], length);
    }
    else
    {
        msg[0].len = length + 2;
        msg[0].buf[0] = (addr >> 8) & 0xff;
        msg[0].buf[1] = addr & 0xff;

        memcpy(&buffer[2], &data[0], length);
    }

    for (retry = 0; retry < MAX_I2C_RETRY_TIME; retry++) {
        if (i2c_transfer(client->adapter, msg, 1) == 1) {
            retval = length;
            break;
        }
        msleep(20);
    }
    if (retry == MAX_I2C_RETRY_TIME) {
        dev_err(&client->dev, "%s: I2C write over retry limit\n", __func__);
        retval = -EIO;
    }

    kfree(buffer);
    return retval;
}

#endif

/**
 * touch_i2c_read_byte - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to read
 *
 * Actully, This function call touch_i2c_read_block for IIC transfer, 
 * Returning zero(transfer success) or most likely negative errno(transfer error)
 */
int touch_i2c_read_byte(struct i2c_client* client, unsigned short addr)
{
    int retval = 0;
    unsigned char buf[2] = {0};    

    if (!client)    {
        dump_stack();
        return -1;
    }
    retval = touch_i2c_read_block(client, addr, 1, buf);
    if (retval >= 0)
        retval = buf[0] & 0xff;        

    return retval;
}    


/**
 * touch_i2c_write_byte - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to write
 * @data: data we want to send
 *
 * Actully, This function call touch_i2c_write_block for IIC transfer, 
 * Returning zero(transfer success) or most likely negative errno(transfer error)
 */
int touch_i2c_write_byte(struct i2c_client* client, unsigned short addr, unsigned char data)
{
    int retval;
    int length_trans = 1;
    unsigned char data_send = data;    

    if (!client)    {
        dump_stack();
        return -EINVAL;
    }
    retval = touch_i2c_write_block(client, addr, length_trans, &data_send);  
    if (retval == length_trans)
        retval = 0;    

    return retval;
}

/**
 * touch_i2c_read_word - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to write
 * @data: data we want to read
 *
 * Actully, This func call touch_i2c_Read_block for IIC transfer, 
 * Returning negative errno else a 16-bit unsigned "word" received from the device.
 */
int touch_i2c_read_word(struct i2c_client* client, unsigned short addr)
{
    int retval;
    unsigned char buf[2] = {0};        

    if (!client)    {
        dump_stack();
        return -EINVAL;
    }
    retval = touch_i2c_read_block(client, addr, 2, buf);
    if (retval >= 0)
        retval = buf[1] << 8 | buf[0];      

    return retval;
}

/**
 * touch_i2c_write_word - Using for "read word" through IIC
 * @client: Handle to slave device
 * @addr: addr to write
 * @data: data we want to send
 *
 * Actully, This function call touch_i2c_write_block for IIC transfer, 
 * Returning zero(transfer success) or most likely negative errno(transfer error)
 */
int touch_i2c_write_word(struct i2c_client* client, unsigned short addr, unsigned short data)
{
    int retval;
    int length_trans = 2;
    unsigned char buf[2] = {data & 0xff, (data >> 8) & 0xff};  

    if (!client)    {
        dump_stack();
        return -EINVAL;
    }

    retval = touch_i2c_write_block(client, addr, length_trans, buf);
    if (retval == length_trans)
        retval = 0;  

    return retval;
}    

/**
 * init_touch_interfaces - Using for Register IIC interface
 * @dev: i2c_client->dev using to alloc memory for dma transfer
 * @flag_register_16bit: bool param to detect whether this device using 16bit IIC address or 8bit address
 *
 * Actully, This function don't have many operation, we just detect device address length && alloc DMA memory for MTK platform
 * Returning zero(sucess) or -ENOMEM(memory alloc failed)
 */
int init_touch_interfaces(struct device *dev, bool flag_register_16bit)
{
    register_is_16bit = flag_register_16bit;

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM  // mtk
    gprDMABuf_va = (u8 *)dma_alloc_coherent(dev, GTP_DMA_MAX_TRANSACTION_LENGTH, &gprDMABuf_pa, GFP_KERNEL);
    if (!gprDMABuf_va) {
        TPD_INFO("[Error] Allocate DMA I2C Buffer failed!\n");
        return -ENOMEM;
    }
    memset(gprDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);

    gpwDMABuf_va = (u8 *)dma_alloc_coherent(dev, GTP_DMA_MAX_TRANSACTION_LENGTH, &gpwDMABuf_pa, GFP_KERNEL);
    if (!gpwDMABuf_va) {
        TPD_INFO("[Error] Allocate DMA I2C Buffer failed!\n");
        return -ENOMEM;
    }
    memset(gpwDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);    
#endif

    return 0;
}

