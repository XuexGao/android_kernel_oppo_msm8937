#ifndef KSU_SUSFS_DEF_H
#define KSU_SUSFS_DEF_H

#include <linux/bits.h>
#include <linux/string.h>

/********/
/* ENUM */
/********/
/* shared with userspace ksu_susfs tool */
#define SUSFS_MAGIC 0xFAFAFAFA
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_ADD_SUS_PATH_LOOP 0x55553
#define CMD_SUSFS_ADD_SUS_MOUNT 0x55560
#define CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561
#define CMD_SUSFS_ADD_SUS_KSTAT 0x55570
#define CMD_SUSFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_SUSFS_ADD_TRY_UMOUNT 0x55580
#define CMD_SUSFS_SET_UNAME 0x55590
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG 0x555b0
#define CMD_SUSFS_ADD_OPEN_REDIRECT 0x555c0
#define CMD_SUSFS_RUN_UMOUNT_FOR_CURRENT_MNT_NS 0x555d0
#define CMD_SUSFS_SHOW_VERSION 0x555e1
#define CMD_SUSFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_SUSFS_SHOW_VARIANT 0x555e3
#define CMD_SUSFS_SHOW_SUS_SU_WORKING_MODE 0x555e4
#define CMD_SUSFS_IS_SUS_SU_READY 0x555f0
#define CMD_SUSFS_SUS_SU 0x60000
#define CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING 0x60010
#define CMD_SUSFS_ADD_SUS_MAP 0x60020

#define SUSFS_MAX_LEN_PATHNAME 256
#define SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE 4096
#define SUSFS_ENABLED_FEATURES_SIZE 8192
#define SUSFS_MAX_VERSION_BUFSIZE 16
#define SUSFS_MAX_VARIANT_BUFSIZE 16

#define TRY_UMOUNT_DEFAULT 0 /* used by susfs_try_umount() */
#define TRY_UMOUNT_DETACH 1 /* used by susfs_try_umount() */

#define SUS_SU_DISABLED 0
#define SUS_SU_WITH_OVERLAY 1 /* deprecated */
#define SUS_SU_WITH_HOOKS 2

#define DEFAULT_SUS_MNT_ID 100000 /* used by mount->mnt_id */
#define DEFAULT_SUS_MNT_ID_FOR_KSU_PROC_UNSHARE 1000000 /* used by vfsmount->susfs_mnt_id_backup */
#define DEFAULT_SUS_MNT_GROUP_ID 1000 /* used by mount->mnt_group_id */

/*
 * inode->i_state => storing flag 'INODE_STATE_'
 * mount->mnt.susfs_mnt_id_backup => storing original mnt_id of normal mounts or custom sus mnt_id of sus mounts
 * task_struct->susfs_last_fake_mnt_id => storing last valid fake_mnt_id
 * task_struct->susfs_task_state => storing flag 'TASK_STRUCT_'
 */

#define INODE_STATE_SUS_PATH BIT(24)
#define INODE_STATE_SUS_MOUNT BIT(25)
#define INODE_STATE_SUS_KSTAT BIT(26)
#define INODE_STATE_OPEN_REDIRECT BIT(27)

#define TASK_STRUCT_NON_ROOT_USER_APP_PROC BIT(24)

#define MAGIC_MOUNT_WORKDIR "/debug_ramdisk/workdir"
#define DATA_ADB_UMOUNT_FOR_ZYGOTE_SYSTEM_PROCESS "/data/adb/susfs_umount_for_zygote_system_process"
#define DATA_ADB_NO_AUTO_ADD_SUS_BIND_MOUNT "/data/adb/susfs_no_auto_add_sus_bind_mount"
#define DATA_ADB_NO_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT "/data/adb/susfs_no_auto_add_sus_ksu_default_mount"
#define DATA_ADB_NO_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT "/data/adb/susfs_no_auto_add_try_umount_for_bind_mount"

/* inline helper functions required by SukiSU-Ultra */
static inline bool susfs_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str++ != *prefix++)
            return false;
    }
    return true;
}

static inline bool susfs_ends_with(const char *str, const char *suffix) {
    size_t str_len, suffix_len;
    if (!str || !suffix)
        return false;
    str_len = strlen(str);
    suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    return !strcmp(str + str_len - suffix_len, suffix);
}

/* stub: marking current proc as umounted - no-op on v1.5.5 mechanism */
static inline void susfs_set_current_proc_umounted(void) {
    /* v1.5.5 uses task_struct->susfs_task_state instead of thread_info flags */
    if (likely(current->susfs_task_state & TASK_STRUCT_NON_ROOT_USER_APP_PROC)) {
        return;
    }
    current->susfs_task_state |= TASK_STRUCT_NON_ROOT_USER_APP_PROC;
}

#endif // #ifndef KSU_SUSFS_DEF_H
