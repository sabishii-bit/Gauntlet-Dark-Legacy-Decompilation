.include "macros.s"

.section .dtors, "wa"  # 0x80110700 - 0x80110720 ; 0x00000020


.global lbl_80110700
lbl_80110700:

	# ROM: 0x10D700
	.4byte func_800E3174
	.4byte lbl_800E5328
	.4byte func_800E3174
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
