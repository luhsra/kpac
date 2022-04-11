#include "common.h"

	# aut
	movq (%rsp), %rdi
	movq %rdi, REG_CIPHER
	movq %rsp, REG_TWEAK
	movq $DEV_AUT, REG_STATE
1:	movq REG_STATE, %rdi
	testq %rdi, %rdi
	jne 1b
	movq REG_PLAIN, %rdi
	movq %rdi, (%rsp)
