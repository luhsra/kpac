#include "../common.h"

	stp	x0, x1, [sp, #-16]

	mov	x0, lr
	mov	x1, sp
	mov	w8, #SYSCALL_PAC
	svc	#0
	mov	lr, x0

	ldp	x0, x1, [sp, #-16]
