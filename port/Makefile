# The port: the game's sources and a runtime standing in for the PlayStation
# (port/runtime), built with Emscripten. From the repository's root:
#   source ~/emsdk/emsdk_env.sh
#   make -f port/Makefile
# Each executable of the disc (title, game, kiosk) becomes a module of its
# own; port/web/vib.js runs them in turn, as the loader does.

CXX             := em++
.DEFAULT_GOAL   := all
BUILD           := build/port
OUT             := port/web/build

GAME_DIR        := src/game
RUNTIME_DIR     := port/runtime

TITLE_SRC       := VideoSys CdSys AudioSys InputSys MemorySys FileSys unk_22EE8 PackedFiles TitleMain \
                   InputControl TmdLineObj unk_25590 CameraMovie unk_25B68 unk_28A88 unk_29B0C Message \
                   FontHack unk_2B054
KIOSK_SRC       := VideoSys CdSys AudioSys InputSys MemorySys FileSys unk_22EE8 PackedFiles KioskMain \
                   InputControl TmdLineObj unk_25590 CameraMovie unk_25B68 unk_28A88 Message FontHack \
                   unk_2B054
GAME_SRC        := FbStream VideoSys CdSys AudioSys Thread InputSys MemorySys FileSys unk_22EE8 Format \
                   GameMain AudioStream SignalModule SignalAnalyzer SignalSys InputControl Parameter Game \
                   Receiver FastAllocator Forms unk_28A88 NumberVoice TitleDraw Font Title AnalysisScript \
                   Gameplay Score SceneDraw Meters RoadSelect Glue TmdLineObj unk_25590 CameraMovie \
                   ScoreMeter RandomPicks PackedFiles GameCamera Message FontHack SoundSequence unk_2B054

RUNTIME_SRC     := sys gpu gs gte cd spu pad
RUNTIME_H       := $(RUNTIME_DIR)/psx.h

OPT             ?= -O2
INCLUDES        := -Iport/include -Iinclude -idirafter include/psyq
# the game's sources: the standard library for SGI STL, and no warnings (it is 1999's C++)
GAME_FLAGS      := $(OPT) -std=gnu++17 -fsigned-char -fno-exceptions -fno-rtti -Wno-everything \
                   -DPORT -DNON_MATCHING -include port/prelude.h $(INCLUDES)
RUNTIME_FLAGS   := $(OPT) -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wno-unused-function \
                   -Wno-unused-variable -Wno-unused-parameter -DPORT $(INCLUDES)

EXPORTS         := _main,_port_loader_data,_port_loader_data_size,_port_loader_restored,_port_set_hq_music,_port_set_gaussian
# 16 MB of memory, which never grows: a primitive's 24-bit link (addPrim) holds any address
LDFLAGS         := $(OPT) -sINITIAL_MEMORY=16MB -sALLOW_MEMORY_GROWTH=0 -sSTACK_SIZE=256KB \
                   -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=1 \
                   -sINVOKE_RUN=0 -sENVIRONMENT=web,worker,node -sEXPORTED_FUNCTIONS=$(EXPORTS) \
                   -sEXPORTED_RUNTIME_METHODS=callMain,HEAPU8 --js-library $(RUNTIME_DIR)/library.js

# $(1): the module, $(2): its sources' list, $(3): its define, $(4): the JavaScript name
define exe
$(1)_OBJ := $$(addprefix $(BUILD)/$(1)/game/,$$(addsuffix .o,$$($(2)_SRC))) \
            $(BUILD)/$(1)/boot.o \
            $$(addprefix $(BUILD)/$(1)/runtime/,$$(addsuffix .o,$$(RUNTIME_SRC)))

$(BUILD)/$(1)/game/%.o: $(GAME_DIR)/%.cpp port/prelude.h
	@mkdir -p $$(dir $$@)
	$(CXX) $(GAME_FLAGS) $(3) -Dmain=port_game_main -c $$< -o $$@

$(BUILD)/$(1)/boot.o: port/src/boot.cpp port/prelude.h $(RUNTIME_H)
	@mkdir -p $$(dir $$@)
	$(CXX) $(GAME_FLAGS) $(3) -I$(GAME_DIR) -c $$< -o $$@

$(BUILD)/$(1)/runtime/%.o: $(RUNTIME_DIR)/%.cpp $(RUNTIME_H)
	@mkdir -p $$(dir $$@)
	$(CXX) $(RUNTIME_FLAGS) $(3) -c $$< -o $$@

$(OUT)/$(1).js: $$($(1)_OBJ) $(RUNTIME_DIR)/library.js port/Makefile
	@mkdir -p $$(dir $$@)
	$(CXX) $$($(1)_OBJ) $(LDFLAGS) -sEXPORT_NAME=$(4) -o $$@
endef

$(eval $(call exe,title,TITLE,,VibTitle))
$(eval $(call exe,game,GAME,-DGAME,VibGame))
$(eval $(call exe,kiosk,KIOSK,-DKIOSK,VibKiosk))

all: $(OUT)/title.js $(OUT)/game.js $(OUT)/kiosk.js
title: $(OUT)/title.js
game: $(OUT)/game.js
kiosk: $(OUT)/kiosk.js

clean:
	rm -rf $(BUILD) $(OUT)

.PHONY: all title game kiosk clean
