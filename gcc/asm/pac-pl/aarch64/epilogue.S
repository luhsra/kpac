#include "common.h"

	mov	x9, #PAC_PL_BASE
1:	mov	x10, sp

	stp	x10, lr, [x9, #PAC_PL_TWEAK]
	ldr	x10, [x9, #PAC_PL_CIPHER]
	cbz	x10, 1b

	mov	lr, x10
