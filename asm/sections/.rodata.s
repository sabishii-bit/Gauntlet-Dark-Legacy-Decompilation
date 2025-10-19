.include "macros.inc"


.section .rodata, "wa"  # 0x80110720 - 0x80118100
	.incbin "baserom.dol", 0x10D720, 0x79E0
