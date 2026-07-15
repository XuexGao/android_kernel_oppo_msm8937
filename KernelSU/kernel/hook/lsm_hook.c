#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI) ||                                     \
    defined(CONFIG_KSU_ALLOWLIST_WORKAROUND)
static int ksu_key_permission(key_ref_t key_ref, const struct cred *cred, unsigned perm)
{
    if (init_session_keyring != NULL) {
        return 0;
    }

    if (strcmp(current->comm, "init")) {
        // we are only interested in `init` process
        return 0;
    }
    init_session_keyring = cred->session_keyring;
    pr_info("kernel_compat: got init_session_keyring\n");
    return 0;
}
#endif

#ifdef CONFIG_KSU_SUSFS
extern u32 susfs_zygote_sid;
extern void disable_seccomp(void);
extern struct work_struct susfs_extra_works;

static inline void ksu_handle_extra_susfs_work(void)
{
    if (work_pending(&susfs_extra_works))
        return;

    schedule_work(&susfs_extra_works);
}

int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    // We only interest in process spawned by zygote
    // If susfs_zygote_sid is initialized, use fast SID comparison.
    // Otherwise fall back to is_zygote() which uses string comparison.
    if (likely(susfs_zygote_sid != 0)) {
        if (!susfs_is_sid_equal(current_cred(), susfs_zygote_sid))
            return 0;
    } else {
        if (!is_zygote(current_cred()))
            return 0;
        pr_warn("ksu_handle_setresuid: susfs_zygote_sid=0, used is_zygote fallback for ruid=%d\n", ruid);
    }

    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_isolated_process(ruid))
        goto do_umount;

    // If manager appid is not yet valid, try to search for manager APK.
    // This handles the bootstrap problem: on a fresh install, ksud doesn't
    // exist yet, so on_boot_completed (which is triggered via ksud ioctl)
    // never fires. We search here because setresuid is called by zygote
    // for every app fork, and by the time system_server is up, packages.list
    // exists and we can find the manager.
    if (unlikely(!ksu_is_manager_appid_valid())) {
        pr_info("ksu_handle_setresuid: manager appid invalid, searching manager...\n");
        track_throne(false);
    }

    // - Since ksu manager app uid is excluded in allow_list_arr, so ksu_uid_should_umount(manager_uid)
    //   will always return true, that's why we need to explicitly check if new_uid belongs to
    //   ksu manager.
    // - Disable seccomp restriction for KSU manager since running with "su" will disable seccomp anyway
    if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", ruid);
        ksu_install_fd();
        return 0;
    }

    // we should not umount for webview zygote
    if (unlikely(ruid == WEBVIEW_ZYGOTE_UID))
        return 0;

    // Check if spawned process is normal user app and needs to be umounted
    if (likely(is_appuid(ruid) && ksu_uid_should_umount(ruid)))
        goto do_umount;

    // - Disable seccomp restriction for root allowed apps since running with "su" will disable seccomp anyway
    if (ksu_is_allow_uid_for_current(ruid))
        disable_seccomp();

    return 0;

do_umount:
    {
        // Handle kernel umount
        ksu_handle_umount(current_uid().val, ruid);

        // Handle extra susfs work
        ksu_handle_extra_susfs_work();
    }

    // Mark current proc as umounted
    susfs_set_current_proc_umounted();

    return 0;
}

// Wrapper to call ksu_handle_setresuid via task_fix_setuid LSM hook.
// In SUSFS mode, ksu_handle_setresuid is defined but was never wired up to any
// hook. This wrapper bridges the task_fix_setuid LSM hook (called during
// setresuid/setuid/seteuid syscalls) to ksu_handle_setresuid so that manager
// detection, seccomp disabling and fd installation actually fire.
static int ksu_task_fix_setuid_susfs(struct cred *new, const struct cred *old, int flags)
{
    if (unlikely(!new || !old))
        return 0;
    return ksu_handle_setresuid(new->uid.val, new->euid.val, new->suid.val);
}
#else
static int ksu_task_fix_setuid(struct cred *new, const struct cred *old, int flags)
{
    uid_t new_uid, old_uid = 0;

    if (unlikely(!new || !old))
        return 0;

    new_uid = new->uid.val;
    old_uid = old->uid.val;

    if (unlikely(is_uid_manager(new_uid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", new_uid);
        ksu_install_fd();
        return 0;
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        disable_seccomp();
    }

    // Handle kernel umount
    ksu_handle_umount(old_uid, new_uid);

    return 0;
}
#endif

static struct security_hook_list ksu_hooks[] = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI) ||                                     \
    defined(CONFIG_KSU_ALLOWLIST_WORKAROUND)
    LSM_HOOK_INIT(key_permission, ksu_key_permission),
#endif
#ifdef CONFIG_KSU_SUSFS
    LSM_HOOK_INIT(task_fix_setuid, ksu_task_fix_setuid_susfs),
#else
    LSM_HOOK_INIT(task_fix_setuid, ksu_task_fix_setuid),
#endif
};

void __init ksu_lsm_hook_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks), "ksu");
#else
    // https://elixir.bootlin.com/linux/v4.10.17/source/include/linux/lsm_hooks.h#L1892
    security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks));
#endif
    pr_info("LSM hooks initialized.\n");
}

void ksu_lsm_hook_exit(void)
{
}
