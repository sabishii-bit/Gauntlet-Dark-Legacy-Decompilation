.include "macros.inc"


.section .dtors, "wa"  # 0x80110700 - 0x80110720
	.incbin "baserom.dol", 0x10D700, 0x20
