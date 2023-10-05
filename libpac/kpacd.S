#define OP_PAC			1
#define OP_AUT			2

#define PAC_BASE		0x9AC00000000

#define REG_PLAIN		8
#define REG_TWEAK		16
#define REG_CIPHER		24

	.section text_pac_procs, "ax"

	/* We cannot make any assumptions about the code's optimization level,
	 * so the compiler may do whatever it pleases with volatile registers,
	 * including not saving them in the caller.  Act pessimistically and
	 * save them on the stack. */

	.global do_pac_8
do_pac_8:
	/* lr is at [sp+8] */
	str	x10, [sp, #-8]
	add	x10, sp, 8
	b	2f

	.global do_pac_0
do_pac_0:
	/* lr is at [sp] */
	str	x10, [sp, #-8]
	mov	x10, sp

2:	stp	x9, x11, [sp, #-24]

	ldr	x9, [x10]
	mov	x11, #PAC_BASE
	stp	x9, x10, [x11, #REG_PLAIN]

	mov	x9, #OP_PAC
	stlr	x9, [x11]

	sevl
1:	wfe
	ldxr	x9, [x11]
	cbnz	x9, 1b

	ldr	x9, [x11, #REG_CIPHER]
	str	x9, [x10]

	ldr	x10, [sp, #-8]
	ldp	x9, x11, [sp, #-24]

	ret

	.global do_aut_8
do_aut_8:
	/* lr is at [sp+8] */
	str	x10, [sp, #-8]
	add	x10, sp, 8
	b	2f

	.global do_aut_0
do_aut_0:
	/* lr is at [sp] */
	str	x10, [sp, #-8]
	mov	x10, sp

2:	stp	x9, x11, [sp, #-24]

	ldr	x9, [x10]
	mov	x11, #PAC_BASE
	stp	x10, x9, [x11, #REG_TWEAK]

	mov	x9, #OP_AUT
	stlr	x9, [x11]

	sevl
1:	wfe
	ldxr	x9, [x11]
	cbnz	x9, 1b

	ldr	x9, [x11, #REG_PLAIN]
	str	x9, [x10]

	ldr	x10, [sp, #-8]
	ldp	x9, x11, [sp, #-24]

	ret
