#include "common.h"

	# aut
	movq (%rsp), %rax
	movq %rax, REG_CIPHER
	movq %rsp, REG_TWEAK
	movq $DEV_AUT, REG_STATE
1:	movq REG_STATE, %rax
	testq %rax, %rax
	jne 1b
	movq REG_PLAIN, %rax
	movq %rax, (%rsp)
