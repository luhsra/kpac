#include "common.h"

	# pac
	movq (%rsp), %rax
	movq %rax, REG_PLAIN
	movq %rsp, REG_TWEAK
	movq $DEV_PAC, REG_STATE
1:	movq REG_STATE, %rax
	testq %rax, %rax
	jne 1b
	movq REG_CIPHER, %rax
	movq %rax, (%rsp)
