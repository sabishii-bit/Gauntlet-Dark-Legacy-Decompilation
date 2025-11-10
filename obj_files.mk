INIT_O_FILES :=                                     \
    $(OBJ_DIR)/asm/sections/init.o

EXTAB_O_FILES :=                                    \
    $(OBJ_DIR)/asm/sections/extab_.o

EXTABINDEX_O_FILES :=                               \
    $(OBJ_DIR)/asm/sections/extabindex_.o

TEXT_O_FILES :=                                     \
    $(OBJ_DIR)/src/text.o                           \
    $(OBJ_DIR)/asm/sections/text_func_wrap.o

CTORS_O_FILES :=                                    \
    $(OBJ_DIR)/asm/sections/ctors.o

DTORS_O_FILES :=                                    \
    $(OBJ_DIR)/asm/sections/dtors.o

RODATA_O_FILES :=                                   \
    $(OBJ_DIR)/asm/sections/rodata.o

DATA_O_FILES :=                                     \
    $(OBJ_DIR)/asm/sections/data.o

BSS_O_FILES :=                                      \
    $(OBJ_DIR)/asm/sections/bss.o

SDATA_O_FILES :=                                    \
    $(OBJ_DIR)/asm/sections/sdata.o

SBSS_O_FILES :=                                     \
    $(OBJ_DIR)/asm/sections/sbss.o

SDATA2_O_FILES :=                                   \
    $(OBJ_DIR)/asm/sections/sdata2.o
