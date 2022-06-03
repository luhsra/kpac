#include "../common.h"

	mov	x9, #PAC_BASE
	mov	x10, sp
	stp	lr, x10, [x9, #REG_PLAIN]

	mov	x10, #OP_PAC
	stlr	x10, [x9]

	sevl
1:	wfe
	ldr	x10, [x9]
	cbnz	x10, 1b
	ldr	lr, [x9, #REG_CIPHER]
