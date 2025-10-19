.include "macros.inc"


.section extab, "wa"  # 0x800054E0 - 0x800087A0
	.incbin "baserom.dol", 0x105C80, 0x32C0
