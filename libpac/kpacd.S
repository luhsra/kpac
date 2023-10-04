	/* = PAC section ================================= */

	.section text_do_pac, "ax"
	.global do_pac_8
	.global do_pac_0

do_pac_8:
	b 1f
	nop
	nop

do_pac_0:
	nop
	nop

1:
	ret

	/* = AUT section ================================= */

	.section text_do_aut, "ax"
	.global do_aut_8
	.global do_aut_0

do_aut_8:
	b 1f
	nop
	nop

do_aut_0:
	nop
	nop
1:

	ret
