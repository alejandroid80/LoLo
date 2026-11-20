CROSS_COMPILE ?= aarch64-nextui-linux-gnu-
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip

SRC := $(shell find /workspace/src -name '*.c' | sort)
INCLUDES := -I/workspace/third_party/apostrophe/include -I/workspace/src
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LDFLAGS := $(shell pkg-config --libs sdl2) -lSDL2_ttf -lm -lpthread -ldl

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
	-mcpu=cortex-a53 -mtune=cortex-a53 -DPLATFORM_TG5040 \
	-ffunction-sections -fdata-sections -fstack-protector-strong \
	$(INCLUDES) $(SDL_CFLAGS)

all: $(BUILD_DIR)/lolo

$(BUILD_DIR)/lolo: $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(SDL_LDFLAGS)
	$(STRIP) $@