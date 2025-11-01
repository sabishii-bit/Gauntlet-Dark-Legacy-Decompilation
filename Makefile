ifneq ($(findstring MINGW,$(shell uname)),)
  WINDOWS := 1
endif
ifneq ($(findstring MSYS,$(shell uname)),)
  WINDOWS := 1
endif
ifneq ($(findstring microsoft,$(shell uname -a)),)
  WINDOWS := 1
endif

# If 0, tells the console to chill out. (Quiets the make process.)
VERBOSE ?= 1

# If GENERATE_MAP set to 1, tells LDFLAGS to generate a mapfile, which makes linking take several minutes.
GENERATE_MAP ?= 0

ifeq ($(VERBOSE),0)
  QUIET := @
endif

#-------------------------------------------------------------------------------
# Files
#-------------------------------------------------------------------------------

# Used for elf2dol
TARGET_COL := wii

OBJ_DIR := obj

SRC_DIRS := src

ASM_DIRS := asm asm/sections

BUILD_DIR := build

# Inputs
S_FILES := $(foreach dir,$(ASM_DIRS),$(wildcard $(dir)/*.s))
C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
CPP_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
LDSCRIPT := asm/ldscript.lcf

# Outputs
DOL     := $(BUILD_DIR)/GUNE5D/main.dol
ELF     := $(DOL:.dol=.elf)
MAP     := $(BUILD_DIR)/GUNE5D/GUNE5D.map

include obj_files.mk

O_FILES := $(INIT_O_FILES) $(EXTAB_O_FILES) $(EXTABINDEX_O_FILES) $(TEXT_O_FILES) \
		   $(CTORS_O_FILES) $(DTORS_O_FILES) $(RODATA_O_FILES) $(DATA_O_FILES)    \
		   $(BSS_O_FILES) $(SDATA_O_FILES) $(SBSS_O_FILES) $(SDATA2_O_FILES)

#-------------------------------------------------------------------------------
# Tools
#-------------------------------------------------------------------------------

# Programs
ifeq ($(WINDOWS),1)
  PYTHON := python
  WINE   :=
else
  PYTHON := python3
  WIBO   := $(shell command -v wibo 2> /dev/null)
  ifdef WIBO
    WINE ?= wibo
  else
    WINE ?= wine
  endif
  # Disable wine debug output for cleanliness
  export WINEDEBUG ?= -all
  # Default devkitPPC path
  DEVKITPPC ?= /opt/devkitpro/devkitPPC
endif
# Windows devkitPPC path (MSYS2)
ifeq ($(WINDOWS),1)
  DEVKITPPC ?= /c/devkitPro/devkitPPC
endif
AS      := $(DEVKITPPC)/bin/powerpc-eabi-as
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy
CC      := $(WINE) tools/mwcc_compiler/2.0/mwcceppc.exe
LD      := $(WINE) tools/mwcc_compiler/2.0/mwldeppc.exe
PPROC   := python3 tools/postprocess.py
GLBLASM := python3 tools/inlineasm/globalasm.py
# ELF2DOL := tools/elf2dol  # Unused - using DTK for elf2dol conversion instead
SHA1SUM := sha1sum
ASMDIFF := ./tools/asmdiff.sh


# Options
#INCLUDES := -ir src -ir include -Iinclude -Iinclude/inline -Iinclude/bink \
  -Iinclude/dolphin -Iinclude/CodeWarrior -Iinclude/rwsdk \
  $(foreach dir,$(SRC_DIRS),-I$(dir))

ASFLAGS := -mgekko -I include -I asm/include -I asm/sections --strip-local-absolute -gdwarf-2
ifeq ($(VERBOSE),0)
  ASFLAGS += -W
endif
LDFLAGS := -maxerrors 1 -nostdlib
ifeq ($(GENERATE_MAP),1)
  LDFLAGS += -map $(MAP)
endif
#CFLAGS  := -g -DGAMECUBE -Cpp_exceptions off -proc gekko -fp hard -fp_contract on -O4,p -maxerrors 1 \
           -pragma "check_header_flags off" -RTTI off -pragma "force_active on" \
           -str reuse,pool,readonly -char unsigned -enum int -use_lmw_stmw on -inline off -nostdinc -i- $(INCLUDES)
PPROCFLAGS := -fsymbol-fixup
DTK := $(firstword $(wildcard tools/dtk.exe tools/dtk))

#-------------------------------------------------------------------------------
# Recipes
#-------------------------------------------------------------------------------

default: all

all: setup $(DOL)

# Setting up conditions for building.
setup:
	@echo "Adjusting project to handle build requirements..."
	@if [ -f tools/mwcc_compiler/2.0/lmgr326b.dll ]; then \
	    mv tools/mwcc_compiler/2.0/lmgr326b.dll tools/mwcc_compiler/2.0/LMGR326B.dll; \
	fi
	@if [ -f tools/mwcc_compiler/2.7/lmgr326b.dll ]; then \
	    mv tools/mwcc_compiler/2.7/lmgr326b.dll tools/mwcc_compiler/2.7/LMGR326B.dll; \
	fi
	@if [ -d build/ ]; then \
	    rm -rf build/; \
	fi
	@mkdir -p build/GUNE5D
	@chmod -R +x tools/
	@echo "Finished adjusting."


ALL_DIRS := $(OBJ_DIR) $(addprefix $(OBJ_DIR)/,$(SRC_DIRS) $(ASM_DIRS))

# Make sure build directory exists before compiling anything
DUMMY != mkdir -p $(ALL_DIRS)

.PHONY: tools

$(DOL): $(ELF)
	@echo " ELF2DOL "$@
	$(QUIET) $(DTK) elf2dol $< $@
	@echo " VERIFY  "$@
	$(QUIET) $(DTK) shasum -c config/GUNE5D/build.sha1

clean:
	rm -f $(DOL) $(ELF) $(MAP) baserom.dump main.dump
	rm -rf .pragma obj
	$(MAKE) -C tools clean

tools:
	$(MAKE) -C tools

inspect:
ifeq ($(WINDOWS),1)
	$(CC) $(CFLAGS) -o inspect.s -S $(subst \,/,$(subst C:\,/c/,$(INSPECT)))
else
	$(CC) $(CFLAGS) -o inspect.s -S $(INSPECT)
endif
	python3 tools/inspect_postprocess.py inspect.s

$(ELF): $(O_FILES) $(LDSCRIPT)
	@echo " LINK    "$@
	$S$(LD) $(LDFLAGS) -o $@ -lcf $(LDSCRIPT) $(O_FILES) 1>&2
# The Metrowerks linker doesn't generate physical addresses in the ELF program headers. This fixes it somehow.
	$S$(OBJCOPY) $@ $@

$(OBJ_DIR)/%.o: %.s
	@echo " AS      "$<
	$S$(AS) $(ASFLAGS) -o $@ $<
	$S$(PPROC) $(PPROCFLAGS) $@

$(OBJ_DIR)/%.o: %.c
	@echo " CC      "$<
	$S$(CC) $(CFLAGS) -c -o $@ $< 1>&2

$(OBJ_DIR)/%.o: %.cpp
	@echo " CXX     "$<
	$S$(GLBLASM) -s $< $(OBJ_DIR)/$*.cpp 1>&2
	$S$(CC) $(CFLAGS) -c -o $@ $(OBJ_DIR)/$*.cpp 1>&2
	$S$(PPROC) $(PPROCFLAGS) $@
