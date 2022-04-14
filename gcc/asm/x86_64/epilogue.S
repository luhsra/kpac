#include "../common.h"

	movq (%rsp), %rdi
	movq %rdi, DEV_CIPHER
	movq %rsp, DEV_TWEAK
	movq $DEV_AUT, DEV_BASE
1:	cmpq $DEV_STANDBY, DEV_BASE
	je 2f
	pause
	jmp 1b
2:	movq DEV_PLAIN, %rdi
	movq %rdi, (%rsp)
