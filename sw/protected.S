	.text

#define DEV_STANDBY	0
#define DEV_PAC		1
#define DEV_AUT		2

DEV_BASE  = 0xA000000
DEV_STATE = DEV_BASE+0
DEV_PLAIN = DEV_BASE+8
DEV_TWEAK = DEV_BASE+16
DEV_CIPH  = DEV_BASE+24

	.global protected
protected:
	movq (%rsp), %rax
	movq %rax, DEV_PLAIN
	movq %rsp, DEV_TWEAK
	movq $DEV_PAC, DEV_STATE
1:	movq DEV_STATE, %rax
	test %rax, %rax
	jne 1b
	movq DEV_CIPH, %rax
	movq %rax, (%rsp)

	push %rbp
	mov %rsp, %rbp

	leave

	movq (%rsp), %rax
	movq %rax, DEV_CIPH
	movq %rsp, DEV_TWEAK
	movq $DEV_AUT, DEV_STATE
1:	movq DEV_STATE, %rax
	test %rax, %rax
	jne 1b
	movq DEV_PLAIN, %rax
	movq %rax, (%rsp)

	ret
