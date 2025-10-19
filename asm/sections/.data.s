.include "macros.inc"


.section .data, "wa"  # 0x80118100 - 0x8023CA40
	.incbin "baserom.dol", 0x115100, 0x124940
