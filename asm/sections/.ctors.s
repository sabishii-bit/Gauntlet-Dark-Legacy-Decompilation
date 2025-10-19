.include "macros.inc"


.section .ctors, "wa"  # 0x801106E0 - 0x80110700
	.incbin "baserom.dol", 0x10D6E0, 0x20
