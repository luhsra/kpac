#include "../common.h"

	/* We could save some values in scratch registers like r10.
	 * Note: r11 and rcx are clobbered by a syscall. */

	mov	%rdi, -8(%rsp)
	mov	%rsi, -16(%rsp)
	mov	%rcx, -24(%rsp)

	movq	(%rsp), %rdi
	movq	%rsp, %rsi
	movq	$SYSCALL_PAC, %rax
	syscall
	movq	%rax, (%rsp)

	mov	-8(%rsp), %rdi
	mov	-16(%rsp), %rsi
	mov	-24(%rsp), %rcx
