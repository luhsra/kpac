#include "../common.h"

	mov	x9, #DEV_BASE
	mov	x10, sp
	stp	lr, x10, [x9, #DEV_PLAIN_OFFSET]

	mov	x10, #DEV_PAC
	stlr	x10, [x9]

	sevl
1:	wfe
	ldr	x10, [x9]
	cbnz	x10, 1b
	ldr	lr, [x9, #DEV_CIPHER_OFFSET]
