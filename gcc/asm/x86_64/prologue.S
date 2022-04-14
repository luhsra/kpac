#include "../common.h"

	movq (%rsp), %rax
	movq %rax, DEV_PLAIN
	movq %rsp, DEV_TWEAK
	movq $DEV_PAC, DEV_BASE
1:	cmpq $DEV_STANDBY, DEV_BASE
	je 2f
	pause
	jmp 1b
2:	movq DEV_CIPHER, %rax
	movq %rax, (%rsp)
