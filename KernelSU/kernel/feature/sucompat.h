#ifndef __KSU_H_SUCOMPAT
#define __KSU_H_SUCOMPAT
#include <asm/ptrace.h>
#include <linux/types.h>
#include "compat/kernel_compat.h"

#ifdef KSU_COMPAT_USE_STATIC_KEY
extern struct static_key_true ksu_su_compat_enabled;
#else
extern bool ksu_su_compat_enabled;
#endif

void ksu_sucompat_init(void);
void ksu_sucompat_exit(void);

// Handler functions exported for hook_manager
//
// faccessat / stat 只有 <6.1 的 ABI 形态：本仓库 fs/open.c 与 fs/stat.c 传的是
// `const char __user **`。上游用 CONFIG_KSU_SUSFS 去选 6.1+ 的
// `struct filename **` 版本，在 4.9 上会解引用用户态字符串（开机 init 崩）。
// 详见 sucompat.c 里 faccessat 上方的说明。
int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags);
int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);

#ifdef CONFIG_KSU_SUSFS
// SUSFS 模式下的 execveat 变体，签名本来就是 struct filename **，与 fs/exec.c 一致。
#else
int ksu_handle_post_execve(int *fd, const char *filename, void *argv, void *envp, int *flags, int *retval);
#endif // #ifdef CONFIG_KSU_SUSFS

#ifdef CONFIG_KSU_TRACEPOINT_HOOK
#include <asm/current.h>
#include "hook/tp_marker.h"

// WARNING! THERE HAVE TRYING TO CALL SYSCALL INTERNALLY
// ENSURE CALL IT ONLY IN TRACEPOINT SYSCALL REDIRECT
long ksu_handle_faccessat_sucompat_internal(int orig_nr, struct pt_regs *regs);
long ksu_handle_stat_sucompat_internal(int orig_nr, struct pt_regs *regs);
long ksu_handle_execve_sucompat_internal(const char __user **filename_user, int orig_nr, struct pt_regs *regs);
long ksu_handle_execveat_sucompat_internal(const char __user **filename_user, int orig_nr, struct pt_regs *regs);

// false for ksu_is_current_proc_unprivillege
// when the check of this flag executed in tracepoint, then mean we MUST be marked, or the code won't be executed
#define ksu_is_current_proc_unprivillege() false
#define ksu_set_current_proc_unprivillege() ksu_clear_task_tracepoint_flag_if_needed(current)
#define ksu_clear_current_proc_unprivillege() ksu_set_task_tracepoint_flag(current)

#elif defined(CONFIG_KSU_SUSFS) // susfs
#include <linux/susfs_def.h>

// sync with manual hook
#define TIF_PROC_IN_KSU_EXECVE 61

#define ksu_is_current_proc_unprivillege susfs_is_current_proc_no_su
#define ksu_set_current_proc_unprivillege susfs_set_current_proc_no_su
#define ksu_clear_current_proc_unprivillege susfs_clear_current_proc_no_su
#else // manual hook

// we have a huge number spare TIFs can use
// https://elixir.bootlin.com/linux/v7.2.2/source/arch/arm64/include/asm/thread_info.h#L90
// https://elixir.bootlin.com/linux/v7.2.2/source/arch/arm/include/asm/thread_info.h#L154
// https://elixir.bootlin.com/linux/v7.2.2/source/arch/x86/include/asm/thread_info.h#L103
// 23 - 31 is spare in arm32  (9 tifs)
// 32 - 63 is spare in arm64  (32 tifs)
// 28 - 31 is spare in x86    (4 tifs)
// 28 - 63 is spare in x86-64 (36 tifs)

// 63 already used as TIF_KSU_DISABLE_ESCAPE_WITH_ROOT (64bit)
// 31 already used as TIF_KSU_DISABLE_ESCAPE_WITH_ROOT (32bit)
// TIF_PROC_IN_KSU_EXECVE may reuse in future? because it only useful when current->in_execve=1
#ifdef CONFIG_64BIT
#define TIF_PROC_NON_PRIVILEGE 62
#define TIF_PROC_IN_KSU_EXECVE 61
#else
#define TIF_PROC_NON_PRIVILEGE 30
#define TIF_PROC_IN_KSU_EXECVE 29
#endif

static inline bool ksu_is_current_proc_unprivillege(void)
{
    return (likely(test_thread_flag(TIF_PROC_NON_PRIVILEGE)));
}

static inline void ksu_set_current_proc_unprivillege(void)
{
    set_thread_flag(TIF_PROC_NON_PRIVILEGE);
}

static inline void ksu_clear_current_proc_unprivillege(void)
{
    clear_thread_flag(TIF_PROC_NON_PRIVILEGE);
}
#endif

#endif
