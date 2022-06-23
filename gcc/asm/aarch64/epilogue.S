#include "../common.h"

	mov	x9, #PAC_BASE
	mov	x10, sp
	stp	x10, lr, [x9, #REG_TWEAK]

	mov	x10, #OP_AUT
	stlr	x10, [x9]

	sevl
1:	wfe
	ldxr	x10, [x9]
	cbnz	x10, 1b
	ldr	lr, [x9, #REG_PLAIN]
