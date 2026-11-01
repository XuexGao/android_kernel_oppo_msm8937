/*
 * drivers/staging/android/ion/compat_ion.c
 *
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "ion.h"
#include "compat_ion.h"
#include "ion_legacy.h"
#include "../uapi/msm_ion.h"

extern int ion_flush_buffer_fd(int fd, enum dma_data_direction dir);

/*
 * Legacy Qualcomm flush ABI, 32-bit layout: the ioctl command number is
 * computed from the compat struct size, so it must be matched here before
 * reaching the native handler.
 */
struct compat_ion_flush_data {
	compat_int_t handle;
	compat_int_t fd;
	compat_uptr_t vaddr;
	compat_uint_t offset;
	compat_uint_t length;
};

#define COMPAT_ION_IOC_CLEAN_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 0, \
					      struct compat_ion_flush_data)
#define COMPAT_ION_IOC_INV_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 1, \
					      struct compat_ion_flush_data)
#define COMPAT_ION_IOC_CLEAN_INV_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 2, \
					      struct compat_ion_flush_data)

struct compat_ion_custom_data {
	compat_uint_t cmd;
	compat_uptr_t arg;
};

#define COMPAT_ION_IOC_CUSTOM	_IOWR(ION_IOC_MAGIC, 6, \
				      struct compat_ion_custom_data)

/* See drivers/staging/android/uapi/ion.h for the definition of these structs */
struct compat_ion_old_allocation_data {
	compat_size_t len;
	compat_size_t align;
	compat_uint_t heap_id_mask;
	compat_uint_t flags;
	compat_int_t handle;
};

struct compat_ion_handle_data {
	compat_int_t handle;
};

#define COMPAT_ION_IOC_ALLOC	_IOWR(ION_IOC_MAGIC, 0, \
				      struct compat_ion_old_allocation_data)
#define COMPAT_ION_IOC_FREE	_IOWR(ION_IOC_MAGIC, 1, \
				      struct compat_ion_handle_data)

static int compat_get_ion_flush_fd(struct compat_ion_flush_data __user *data32)
{
	compat_int_t fd, handle;
	int ret;

	ret = get_user(fd, &data32->fd);
	if (ret)
		return ret;

	/*
	 * Pre-4.12 clients keep the dma-buf fd they got from the legacy
	 * alloc ioctl in the handle field; the fd field stays empty.
	 */
	if (!fd) {
		ret = get_user(handle, &data32->handle);
		if (ret)
			return ret;
		fd = handle;
	}

	return fd;
}

static int compat_get_ion_allocation_data(
			struct compat_ion_old_allocation_data __user *data32,
			struct ion_old_allocation_data __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	compat_int_t i;
	int err;

	err = get_user(s, &data32->len);
	err |= put_user(s, &data->len);
	err |= get_user(s, &data32->align);
	err |= put_user(s, &data->align);
	err |= get_user(u, &data32->heap_id_mask);
	err |= put_user(u, &data->heap_id_mask);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);
	err |= get_user(i, &data32->handle);
	err |= put_user(i, &data->handle);

	return err;
}

static int compat_get_ion_handle_data(
			struct compat_ion_handle_data __user *data32,
			struct ion_handle_data __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &data32->handle);
	err |= put_user(i, &data->handle);

	return err;
}

static int compat_put_ion_allocation_data(
			struct compat_ion_old_allocation_data __user *data32,
			struct ion_old_allocation_data __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	compat_int_t i;
	int err;

	err = get_user(s, &data->len);
	err |= put_user(s, &data32->len);
	err |= get_user(s, &data->align);
	err |= put_user(s, &data32->align);
	err |= get_user(u, &data->heap_id_mask);
	err |= put_user(u, &data32->heap_id_mask);
	err |= get_user(u, &data->flags);
	err |= put_user(u, &data32->flags);
	err |= get_user(i, &data->handle);
	err |= put_user(i, &data32->handle);

	return err;
}

long compat_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ION_IOC_ALLOC:
	{
		struct compat_ion_old_allocation_data __user *data32;
		struct ion_old_allocation_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_ion_allocation_data(data32, data);
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, ION_OLD_IOC_ALLOC,
							(unsigned long)data);
		err = compat_put_ion_allocation_data(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_ION_IOC_FREE:
	{
		struct compat_ion_handle_data __user *data32;
		struct ion_handle_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_ion_handle_data(data32, data);
		if (err)
			return err;

		return filp->f_op->unlocked_ioctl(filp, ION_IOC_FREE,
							(unsigned long)data);
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
	case ION_IOC_IMPORT:
	case ION_IOC_SYNC:
	case ION_IOC_ALLOC:
	case ION_IOC_HEAP_QUERY:
	case ION_IOC_PREFETCH:
	case ION_IOC_DRAIN:
		return filp->f_op->unlocked_ioctl(filp, cmd,
						(unsigned long)compat_ptr(arg));
	case COMPAT_ION_IOC_CLEAN_CACHES:
	case COMPAT_ION_IOC_INV_CACHES:
	case COMPAT_ION_IOC_CLEAN_INV_CACHES: {
		struct compat_ion_flush_data __user *data32 =
			compat_ptr(arg);
		int fd;
		enum dma_data_direction dir;

		if (cmd == COMPAT_ION_IOC_CLEAN_CACHES)
			dir = DMA_TO_DEVICE;
		else if (cmd == COMPAT_ION_IOC_INV_CACHES)
			dir = DMA_FROM_DEVICE;
		else
			dir = DMA_BIDIRECTIONAL;

		fd = compat_get_ion_flush_fd(data32);
		if (fd < 0)
			return fd;

		return ion_flush_buffer_fd(fd, dir);
	}
	case COMPAT_ION_IOC_CUSTOM:
	{
		struct compat_ion_custom_data __user *d32 = compat_ptr(arg);
		compat_uint_t mcmd;
		compat_uptr_t uarg;

		if (copy_from_user(&mcmd, &d32->cmd, sizeof(mcmd)) ||
		    copy_from_user(&uarg, &d32->arg, sizeof(uarg)))
			return -EFAULT;
		return ion_legacy_cache_op(mcmd, (unsigned long)uarg);
	}
	default:
		/*
		 * Legacy Qualcomm flush commands. The vendor gralloc was
		 * built against a different struct layout than the one
		 * we declare, so match on the command number instead of
		 * the encoded size.
		 */
		if (_IOC_TYPE(cmd) == ION_IOC_MSM_MAGIC) {
			struct compat_ion_flush_data __user *data32 =
				compat_ptr(arg);
			int fd;
			enum dma_data_direction dir;

			switch (_IOC_NR(cmd)) {
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
				return -ENOIOCTLCMD;
			}

			fd = compat_get_ion_flush_fd(data32);
			if (fd < 0)
				return fd;

			return ion_flush_buffer_fd(fd, dir);
		}
		return -ENOIOCTLCMD;
	}
}
