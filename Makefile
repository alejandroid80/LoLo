APP_NAME := lolo
PAK_NAME := LoLo
APOSTROPHE_DIR := third_party/apostrophe
BUILD_DIR := build

SRC_FILES := $(shell find src -name '*.c' | sort)

native: mac
mac:
	@mkdir -p $(BUILD_DIR)/mac
	cc -std=gnu11 -O0 -g -DPLATFORM_MAC \
		-I$(APOSTROPHE_DIR)/include -Isrc \
		$(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
		-o $(BUILD_DIR)/mac/$(APP_NAME) $(SRC_FILES) \
		$(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image) -lm -lpthread
	@echo "Built native"

tg5040:
	@mkdir -p $(BUILD_DIR)/tg5040
	docker run --rm -v "$(CURDIR)":/workspace \
		ghcr.io/loveretro/tg5040-toolchain:latest \
		make -C /workspace -f ports/tg5040/Makefile BUILD_DIR=/workspace/$(BUILD_DIR)/tg5040
	@mkdir -p LoLo.pak/bin
	@cp $(BUILD_DIR)/tg5040/$(APP_NAME) LoLo.pak/bin/
	@echo "Built for tg5040 -> LoLo.pak/bin/"

clean:
	rm -rf $(BUILD_DIR)