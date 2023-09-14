#include "../common.h"

	stp	x0, x1, [sp, #-16]
	str	x8, [sp, #-24]	/* Do we really need to save it? */

	mov	x0, lr
	mov	x1, sp
	mov	w8, #SYSCALL_AUT
	svc	#0
	mov	lr, x0

	str	x8, [sp, #-24]
	ldp	x0, x1, [sp, #-16]
