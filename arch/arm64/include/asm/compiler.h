/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_COMPILER_H
#define __ASM_COMPILER_H

#ifdef ARM64_ASM_ARCH
#define ARM64_ASM_PREAMBLE ".arch " ARM64_ASM_ARCH "\n"
#else
#define ARM64_ASM_PREAMBLE
#endif

/*
 * 下游 4.19 的 scm.c 用 __asmeq() 做编译期寄存器名校验（do-while 展开时
 * 检查 %0..%N 绑定的确实是 x0..xN）。上游 5.x 之后删掉了 aarch64 的这份
 * 定义（只留了 arch/arm），但我们要搬的 scm.c 依赖它，故补回。
 */
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"


#define xpaclri(ptr)							\
({									\
	register unsigned long __xpaclri_ptr asm("x30") = (ptr);	\
									\
	asm(								\
	ARM64_ASM_PREAMBLE						\
	"	hint	#7\n"						\
	: "+r" (__xpaclri_ptr));					\
									\
	__xpaclri_ptr;							\
})

#ifdef CONFIG_ARM64_PTR_AUTH_KERNEL
#define ptrauth_strip_kernel_insn_pac(ptr)	xpaclri(ptr)
#else
#define ptrauth_strip_kernel_insn_pac(ptr)	(ptr)
#endif

#ifdef CONFIG_ARM64_PTR_AUTH
#define ptrauth_strip_user_insn_pac(ptr)	xpaclri(ptr)
#else
#define ptrauth_strip_user_insn_pac(ptr)	(ptr)
#endif

#if !defined(CONFIG_BUILTIN_RETURN_ADDRESS_STRIPS_PAC)
#define __builtin_return_address(val)					\
	(void *)(ptrauth_strip_kernel_insn_pac((unsigned long)__builtin_return_address(val)))
#endif

#endif /* __ASM_COMPILER_H */
