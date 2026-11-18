#ifndef __KSU_H_SUCOMPAT
#define __KSU_H_SUCOMPAT
#include <asm/ptrace.h>
#include <linux/types.h>

extern bool ksu_su_compat_enabled;

/*
 * faccessat / stat 只有 <6.1 的 ABI 形态：本仓库 fs/open.c 的
 * SYSCALL_DEFINE3(faccessat) 与 fs/stat.c 的 vfs_statx 传的都是
 * `const char __user *filename`。上游（含 SukiSU-Nova builtin）用
 * CONFIG_KSU_SUSFS 去选 6.1+ 的 `struct filename **` 版本，在 4.9 上会把
 * 用户态路径字符串的首 8 字节当成 struct filename 解引用，init 阶段就 Oops
 * （坏地址是 "/init" 的 ASCII，PC=ksu_handle_faccessat）。
 * 所以这两个声明与实现都固定走 4.9 ABI，与 CONFIG_KSU_SUSFS 无关；
 * 具体说明见 sucompat.c 里 `#if 0` 那段。
 */
int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags);
int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);

#ifdef CONFIG_KSU_SUSFS
int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
                 void *argv_user, void *envp_user,
                 int *flags);
#else
int ksu_handle_execve_sucompat(int *fd, const char __user **filename_user, void *argv, void *__never_use_envp,
                               int *__never_use_flags);
int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr, void *argv, void *__never_use_envp,
                                 int *__never_use_flags);
#endif

void ksu_sucompat_init(void);
void ksu_sucompat_exit(void);

#endif