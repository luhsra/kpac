#include "../common.h"

	/* Epilogue is special: we have a return value now.
	 * Also make sure to save arguments in case of a tailcall */
	movq	%rdi, -8(%rsp)
	movq	%rsi, -16(%rsp)
	movq	%rcx, -24(%rsp)
	movq	%rax, -32(%rsp)	/* Also save rax return value */

	movq	(%rsp), %rdi
	movq	%rsp, %rsi
	movq	$SYSCALL_AUT, %rax
	syscall
	movq	%rax, (%rsp)

	movq	-8(%rsp), %rdi
	movq	-16(%rsp), %rsi
	movq	-24(%rsp), %rcx
	movq	-32(%rsp), %rax
