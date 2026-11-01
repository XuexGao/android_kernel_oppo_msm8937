// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "ion.h"
#include "ion_system_secure_heap.h"

#ifdef CONFIG_ION_LEGACY
#include "ion_legacy.h"
#endif


/*
 * Legacy Qualcomm flush API (ION_IOC_MSM_MAGIC 'M') used by old vendor
 * gralloc/memalloc to keep CPU/GPU caches coherent on a dmabuf fd.
 * CLEAN = writeback (DMA_TO_DEVICE), INV = invalidate (DMA_FROM_DEVICE),
 * CLEAN_INV = both directions.
 */
int ion_flush_buffer_fd(int fd, enum dma_data_direction dir)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct scatterlist *sg;
	struct device *dev;
	int i;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	buffer = dmabuf->priv;
	if (!buffer || !buffer->sg_table) {
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	dev = buffer->dev->dev.this_device;

	for_each_sg(buffer->sg_table->sgl, sg, buffer->sg_table->nents, i) {
		if (dir == DMA_TO_DEVICE)
			dma_sync_sg_for_device(dev, sg, 1, dir);
		else if (dir == DMA_FROM_DEVICE)
			dma_sync_sg_for_cpu(dev, sg, 1, dir);
		else {
			dma_sync_sg_for_cpu(dev, sg, 1, dir);
			dma_sync_sg_for_device(dev, sg, 1, dir);
		}
	}

	dma_buf_put(dmabuf);
	return 0;
}

#ifdef CONFIG_ION_LEGACY
/*
 * Vendor cache maintenance dispatched through ION_IOC_CUSTOM: mcmd is
 * one of the M-magic flush ioctls (direction in the command number),
 * uarg points at the vendor ion_flush_data whose first field is the
 * dmabuf fd.
 */
int ion_legacy_cache_op(unsigned int mcmd, unsigned long uarg)
{
	struct ion_flush_data flush;
	enum dma_data_direction dir;
	unsigned int sz;
	int fd;

	if (_IOC_TYPE(mcmd) != ION_IOC_MSM_MAGIC)
		return -ENOTTY;

	switch (_IOC_NR(mcmd)) {
	case 0:
		dir = DMA_TO_DEVICE;
		break;
	case 1:
		dir = DMA_FROM_DEVICE;
		break;
	case 2:
		dir = DMA_BIDIRECTIONAL;
		break;
	default:
		return -ENOTTY;
	}

	memset(&flush, 0, sizeof(flush));
	sz = _IOC_SIZE(mcmd);
	if (sz > sizeof(flush))
		sz = sizeof(flush);
	if (sz && copy_from_user(&flush, (void __user *)uarg, sz))
		return -EFAULT;

	fd = flush.handle;	/* vendor flush_data: fd comes first */
	if (!fd)
		fd = flush.fd;	/* tolerate the uapi field order */

	return ion_flush_buffer_fd(fd, dir);
}
#endif

union ion_ioctl_arg {
	struct ion_allocation_data allocation;
	struct ion_heap_query query;
	struct ion_prefetch_data prefetch_data;
	struct ion_flush_data flush;
#ifdef CONFIG_ION_LEGACY
	struct ion_fd_data fd;
	struct ion_custom_data custom;
	struct ion_old_allocation_data old_allocation;
	struct ion_handle_data handle;
#endif
};

static int validate_ioctl_arg(unsigned int cmd, union ion_ioctl_arg *arg)
{
	switch (cmd) {
	case ION_IOC_HEAP_QUERY:
		if (arg->query.reserved0 ||
		    arg->query.reserved1 ||
		    arg->query.reserved2)
			return -EINVAL;
		break;
	default:
		break;
	}

	return 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
#ifdef CONFIG_ION_LEGACY
	case ION_IOC_FREE:
		return _IOC_WRITE;
#endif
	default:
		return _IOC_DIR(cmd);
	}
}

long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int dir;
	union ion_ioctl_arg data;

	dir = ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	ret = validate_ioctl_arg(cmd, &data);
	if (ret) {
		pr_warn_once("%s: ioctl validate failed\n", __func__);
		return ret;
	}

	if (!(dir & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		int fd;

		fd = ion_alloc_fd(data.allocation.len,
				  data.allocation.heap_id_mask,
				  data.allocation.flags);
		if (fd < 0)
			return fd;

		data.allocation.fd = fd;

		break;
	}
	case ION_IOC_HEAP_QUERY:
		ret = ion_query_heaps(&data.query);
		break;
	case ION_IOC_PREFETCH:
	{
		int ret;

		ret = ion_walk_heaps(data.prefetch_data.heap_id,
				     (enum ion_heap_type)
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_prefetch);
		if (ret)
			return ret;
		break;
	}
	case ION_IOC_DRAIN:
	{
		int ret;

		ret = ion_walk_heaps(data.prefetch_data.heap_id,
				     (enum ion_heap_type)
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_drain);

		if (ret)
			return ret;
		break;
	}
	case ION_IOC_CLEAN_CACHES:
		ret = ion_flush_buffer_fd(data.flush.fd,
					  DMA_TO_DEVICE);
		break;
	case ION_IOC_INV_CACHES:
		ret = ion_flush_buffer_fd(data.flush.fd,
					  DMA_FROM_DEVICE);
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		ret = ion_flush_buffer_fd(data.flush.fd,
					  DMA_BIDIRECTIONAL);
		break;
#ifdef CONFIG_ION_LEGACY
	case ION_OLD_IOC_ALLOC:
	{
		int fd;

		fd = ion_alloc_fd(data.old_allocation.len,
				  data.old_allocation.heap_id_mask,
				  data.old_allocation.flags);
		if (fd < 0)
			return fd;

		data.old_allocation.handle = fd;

		break;
	}
	case ION_IOC_FREE:
		/*
		 * libion passes 0 as the handle to check for this ioctl's
		 * existence and expects -ENOTTY on kernel 4.12+ as an indicator
		 * of having a new ION ABI. We want to use new ION as much as
		 * possible, so pretend that this ioctl doesn't exist when
		 * libion checks for it.
		 */
		if (!data.handle.handle)
			ret = -ENOTTY;

		break;
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
		data.fd.fd = data.fd.handle;
		break;
	case ION_IOC_IMPORT:
		data.fd.handle = data.fd.fd;
		break;
	case ION_IOC_SYNC:
		/*
		 * 4.9 ion_sync_for_device() pushed the cpu cache contents
		 * of the whole dmabuf towards the device. Legacy handle
		 * based clients pass the dma-buf fd in the handle field.
		 */
		if (data.fd.fd)
			ret = ion_flush_buffer_fd(data.fd.fd,
						  DMA_BIDIRECTIONAL);
		else
			ret = ion_flush_buffer_fd(data.fd.handle,
						  DMA_BIDIRECTIONAL);
		break;
		case ION_IOC_CUSTOM:
		/*
		 * 4.9 vendor gralloc/memalloc wraps the M-magic flush
		 * commands in ION_IOC_CUSTOM; the referenced flush data
		 * has the dmabuf fd in its first field.
		 */
		ret = ion_legacy_cache_op(data.custom.cmd,
					  data.custom.arg);
		break;
#endif
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}
