.include "macros.s"

.section .ctors, "wa"  # 0x801106E0 - 0x80110700 ; 0x00000020


.global lbl_801106E0
lbl_801106E0:

	# ROM: 0x10D6E0
	.4byte lbl_800E535C
	.4byte lbl_800B51F4
	.4byte lbl_800EADFC
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
