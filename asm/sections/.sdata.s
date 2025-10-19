.include "macros.inc"


.section .sdata, "wa"  # 0x80343B00 - 0x80344160
	.incbin "baserom.dol", 0x239A40, 0x660
