#include "../common.h"

	movq	$PAC_BASE, %r10
	movq	(%rsp), %r11
	movq	%r11, REG_PLAIN(%r10)
	movq	%rsp, REG_TWEAK(%r10)
	movq	$OP_PAC, (%r10)

1:	cmpq	$0, (%r10)
	je	2f
	pause
	jmp	1b

2:	movq	REG_CIPHER(%r10), %r11
	movq	%r11, (%rsp)
