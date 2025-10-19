.include "macros.inc"


.section .data9, "wa"  # 0x80345720 - 0x80349720
	.incbin "baserom.dol", 0x23A0A0, 0x4000
