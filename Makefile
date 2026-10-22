SHELL           := /bin/bash -e -o pipefail

GAME_ID         := sces028
BUILD_DIR       := build
CONFIG_DIR      := config

# tools
CROSS           := mips-linux-gnu-
AS              := $(CROSS)as -EL -32 -march=r3000 -mtune=r3000 -msoft-float -no-pad-sections -Iinclude/
LD              := $(CROSS)ld -EL
OBJCOPY         := $(CROSS)objcopy
PYTHON          := python3
SPLAT_APP       := tools/splat/split.py
SPLAT           := $(PYTHON) $(SPLAT_APP)
MASPSX_APP      := tools/maspsx/maspsx.py

# The compiler is egcs-2.91.66, see bin/README.md. bin/cc1plus is the build
# configured for mips-sony-psx: like the PSY-Q compiler, it clears and copies
# memory with memset() and memcpy(), and joins generated names with '.'. It
# has one change: like the game's compiler (and GCC 2.95), it emits inline
# virtual functions under -fno-implement-inlines. bin/as runs maspsx, then
# GNU as.
CXX             := ./bin/g++ -c -B./bin/
CXX_FLAGS       := -mips1 -mno-abicalls -mel -msplit-addresses -mgpOPT -mgpopt -msoft-float -g0
CXX_FLAGS       += -Iinclude/stl_psx -Iinclude/stl -Iinclude -Iinclude/psyq -undef -Wall -fno-builtin -fsigned-char -fno-exceptions -fno-rtti -fno-implement-inlines -fcommon
CXX_FLAGS       += -Dmips -D__GNUC__=2 -D__GNUC_MINOR__=91 -D__OPTIMIZE__ -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL
# SGI STL configuration: plain malloc allocator, no <exception> header
CXX_FLAGS       += -D__USE_MALLOC -D__STL_USE_SGI_ALLOCATORS -D__STL_NO_EXCEPTION_HEADER
AS_FLAGS        := -Wa,-EL,-march=r3000,-mtune=r3000,-msoft-float,-no-pad-sections,-Iinclude
SDATA_LIMIT     := -G8
OPT_FLAGS       := -Os

# decomp-permuter's import.py runs make with PERMUTER=1: no INCLUDE_ASM
ifdef PERMUTER
CXX_FLAGS       += -DPERMUTER
endif

# the objects a linker script (written by splat) names, under a directory
ld_objects = $(if $(wildcard $(1)),$(shell tr -s ' ()' '\n\n\n' < $(1) | grep '^$(2)/.*\.o$$' | sort -u))

# link $(1).ld into $(2), with a map in $(3)
define link
	$(LD) -o $(2) \
		-Map $(3) \
		-T $(1).ld \
		-T $(CONFIG_DIR)/undefined_syms_auto.$(GAME_ID).$(1).txt \
		-T $(CONFIG_DIR)/undefined_funcs_auto.$(GAME_ID).$(1).txt \
		-T $(CONFIG_DIR)/undefined_syms.$(GAME_ID).$(1).txt \
		--no-check-sections \
		-nostdlib \
		-s
endef

all: build check

clean:
	rm -rf $(BUILD_DIR) asm vib-ribbon.ld main_k.ld main_g.ld loader.ld $(CONFIG_DIR)/undefined_*_auto.*.txt

# a copy of the four matching builds, for asm-differ's -o
expected: check kiosk-check game-check loader-check
	rm -rf expected/$(BUILD_DIR)
	mkdir -p expected
	cp -r $(BUILD_DIR) expected/$(BUILD_DIR)

# the submodule, if the clone was not --recursive
$(SPLAT_APP):
	git submodule update --init tools/splat

# ---------------------------------------------------------------------------
# MAIN_T.EXE, the title (splat's name for it: vib-ribbon)
TITLE           := $(BUILD_DIR)/MAIN_T.EXE
TITLE_O_FILES    = $(call ld_objects,vib-ribbon.ld,$(BUILD_DIR))

build: $(TITLE)
check: $(TITLE)
	sha1sum --check $(GAME_ID).sha
extract: $(SPLAT_APP)
	$(SPLAT) $(CONFIG_DIR)/splat.$(GAME_ID).yml

$(TITLE): $(BUILD_DIR)/MAIN_T.elf
	$(OBJCOPY) -O binary $< $@
$(BUILD_DIR)/MAIN_T.elf: $(TITLE_O_FILES)
	$(call link,vib-ribbon,$@,$(BUILD_DIR)/vib-ribbon.map)

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -o $@ $<

$(BUILD_DIR)/%.cpp.o: %.cpp $(MASPSX_APP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(SDATA_LIMIT) $(OPT_FLAGS) $(AS_FLAGS) $< -o $@

# ---------------------------------------------------------------------------
# MAIN_K.EXE, the kiosk (demo): the same sources, built with KIOSK
KIOSK           := $(BUILD_DIR)/main_k/MAIN_K.EXE
KIOSK_BUILD     := $(BUILD_DIR)/main_k
KIOSK_O_FILES    = $(call ld_objects,main_k.ld,$(KIOSK_BUILD))

kiosk: $(KIOSK)
kiosk-check: $(KIOSK)
	sha1sum --check $(GAME_ID).main_k.sha
kiosk-extract: $(SPLAT_APP)
	$(SPLAT) $(CONFIG_DIR)/splat.$(GAME_ID).main_k.yml

$(KIOSK): $(KIOSK_BUILD)/MAIN_K.elf
	$(OBJCOPY) -O binary $< $@
$(KIOSK_BUILD)/MAIN_K.elf: $(KIOSK_O_FILES)
	$(call link,main_k,$@,$(KIOSK_BUILD)/main_k.map)

$(KIOSK_BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -o $@ $<

$(KIOSK_BUILD)/%.cpp.o: %.cpp $(MASPSX_APP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) -DKIOSK $(SDATA_LIMIT) $(OPT_FLAGS) $(AS_FLAGS) $< -o $@

# ---------------------------------------------------------------------------
# MAIN_G.EXE, the game itself: the same sources and its own, built with GAME
GAMEPLAY        := $(BUILD_DIR)/main_g/MAIN_G.EXE
GAMEPLAY_BUILD  := $(BUILD_DIR)/main_g
GAMEPLAY_O_FILES = $(call ld_objects,main_g.ld,$(GAMEPLAY_BUILD))

game: $(GAMEPLAY)
game-check: $(GAMEPLAY)
	sha1sum --check $(GAME_ID).main_g.sha
game-extract: $(SPLAT_APP)
	$(SPLAT) $(CONFIG_DIR)/splat.$(GAME_ID).main_g.yml
	@# .sbss starts right after .sdata, which is only 4-aligned
	sed -i 's/main_g_bss (NOLOAD)/main_g_bss ADDR(.main_g) + SIZEOF(.main_g) (NOLOAD)/' main_g.ld

$(GAMEPLAY): $(GAMEPLAY_BUILD)/MAIN_G.elf
	$(OBJCOPY) -O binary --pad-to 0x5E000 $< $@
$(GAMEPLAY_BUILD)/MAIN_G.elf: $(GAMEPLAY_O_FILES)
	$(call link,main_g,$@,$(GAMEPLAY_BUILD)/main_g.map)

$(GAMEPLAY_BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -o $@ $<

# `make game DEBUG_MENU=1`: select on the first controller opens the game's
# hidden menu of controller names, and with it the debug menus (see
# docs/debug_menus.md). Not the original game: `make game-check DEBUG_MENU=1`
# fails.
DEBUG_MENU      ?= 0
$(GAMEPLAY_BUILD)/src/game/InputControl.cpp.o: CXX_FLAGS += $(if $(filter 1,$(DEBUG_MENU)),-DDEBUG_MENU)
$(GAMEPLAY_BUILD)/src/game/InputControl.cpp.o: $(GAMEPLAY_BUILD)/debug_menu.$(DEBUG_MENU)
# InputControl.cpp is built again when DEBUG_MENU changes
$(GAMEPLAY_BUILD)/debug_menu.%:
	@mkdir -p $(dir $@)
	@rm -f $(GAMEPLAY_BUILD)/debug_menu.*
	@touch $@

# the game's SoundDelay::Push() computes its store address anew each time round its loop
$(GAMEPLAY_BUILD)/src/game/AudioStream.cpp.o: CXX_FLAGS += -fno-strength-reduce
$(GAMEPLAY_BUILD)/%.cpp.o: %.cpp $(MASPSX_APP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) -DGAME $(SDATA_LIMIT) $(OPT_FLAGS) $(AS_FLAGS) $< -o $@

# ---------------------------------------------------------------------------
# SCES_028.73, the loader that runs the other three, built with LOADER
LOADER          := $(BUILD_DIR)/loader/SCES_028.73
LOADER_BUILD    := $(BUILD_DIR)/loader
LOADER_O_FILES   = $(call ld_objects,loader.ld,$(LOADER_BUILD))

loader: $(LOADER)
loader-check: $(LOADER)
	sha1sum --check $(GAME_ID).loader.sha
loader-extract: $(SPLAT_APP)
	$(SPLAT) $(CONFIG_DIR)/splat.$(GAME_ID).loader.yml
	@# .bss starts right after .sdata, which is only 4-aligned
	sed -i 's/loader_bss (NOLOAD)/loader_bss ADDR(.loader) + SIZEOF(.loader) (NOLOAD)/' loader.ld

$(LOADER): $(LOADER_BUILD)/SCES_028.73.elf
	$(OBJCOPY) -O binary --pad-to 0x7000 $< $@
$(LOADER_BUILD)/SCES_028.73.elf: $(LOADER_O_FILES)
	$(call link,loader,$@,$(LOADER_BUILD)/loader.map)

$(LOADER_BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -o $@ $<

$(LOADER_BUILD)/%.cpp.o: %.cpp $(MASPSX_APP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) -DLOADER $(SDATA_LIMIT) $(OPT_FLAGS) $(AS_FLAGS) $< -o $@

.PHONY: all clean expected build check extract
.PHONY: kiosk kiosk-check kiosk-extract game game-check game-extract loader loader-check loader-extract
