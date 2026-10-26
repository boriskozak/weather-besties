# Desktop renderer for the Weather Besties UI. Needs gcc + python3 (pillow).
ROOT     := ..
LVGL_DIR := $(ROOT)/components/lvgl
UI_DIR   := $(ROOT)/components/wb_ui
CJSON    := cjson

SRC := $(shell find $(LVGL_DIR)/src -name '*.c') \
       $(UI_DIR)/ui.c $(UI_DIR)/wx.c $(UI_DIR)/outfit.c $(UI_DIR)/cat.c $(UI_DIR)/cal.c $(UI_DIR)/update.c $(wildcard $(UI_DIR)/assets/*.c) \
       $(CJSON)/cJSON.c sim_main.c
OBJ := $(patsubst %.c,build/%.o,$(subst $(ROOT)/,root/,$(SRC)))

CFLAGS := -O2 -g -pthread -I$(ROOT)/components -I$(LVGL_DIR) -I$(UI_DIR) -I$(CJSON) \
          -DLV_LVGL_H_INCLUDE_SIMPLE -DLV_CONF_INCLUDE_SIMPLE -DLV_KCONFIG_IGNORE -Wno-unused-result -DWB_SIM \
          -MMD -MP   # rebuild a file when a header it uses changes

render: $(OBJ)
	$(CC) -o $@ $^ -lm -pthread

-include $(OBJ:.o=.d)

build/root/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

shots: render
	python3 make_scenarios.py
	mkdir -p out
	./render out scenarios
	python3 to_png.py out
	python3 check_renders.py out

# Checks every request the board makes fits the HTTP client's send buffer,
# that the calendar parser stores the right instants (iter-29), and that the
# OTA update manifest parser and version comparison behave (iter-32).
test: test_request test_cal test_update
	./test_request
	./test_cal
	./test_update

test_request: test_request.c $(UI_DIR)/wb_request.c $(UI_DIR)/wb_request.h
	$(CC) -O2 -Wall -Wextra -I$(UI_DIR) -o $@ test_request.c $(UI_DIR)/wb_request.c

test_cal: test_cal.c $(UI_DIR)/cal.c $(UI_DIR)/cal.h
	$(CC) -O2 -Wall -Wextra -DWB_SIM -I$(UI_DIR) -o $@ test_cal.c $(UI_DIR)/cal.c

test_update: test_update.c $(UI_DIR)/update.c $(UI_DIR)/update.h
	$(CC) -O2 -Wall -Wextra -I$(UI_DIR) -I$(CJSON) -o $@ test_update.c $(UI_DIR)/update.c $(CJSON)/cJSON.c

clean:
	rm -rf build render out test_request test_cal test_update


# ---- Browser emulator (WebAssembly) ----
# Needs wasi-sdk (https://github.com/WebAssembly/wasi-sdk). Point WASI_SDK at it.
WASI_SDK ?= /home/claude/wasm/wasi-sdk-33.0-x86_64-linux
WCC      := $(WASI_SDK)/bin/clang --target=wasm32-wasip1 --sysroot=$(WASI_SDK)/share/wasi-sysroot
WEB_SRC  := $(shell find $(LVGL_DIR)/src -name '*.c') \
            $(UI_DIR)/ui.c $(UI_DIR)/wx.c $(UI_DIR)/outfit.c $(UI_DIR)/cat.c $(UI_DIR)/cal.c $(UI_DIR)/wb_request.c \
            $(wildcard $(UI_DIR)/assets/*.c) $(CJSON)/cJSON.c web/emu_main.c
WEB_OBJ  := $(patsubst %.c,build-web/%.o,$(subst $(ROOT)/,root/,$(WEB_SRC)))
WEB_CFLAGS := -O2 -MMD -MP -I$(ROOT)/components -I$(LVGL_DIR) -I$(UI_DIR) -I$(CJSON) -Iweb \
              -DLV_LVGL_H_INCLUDE_SIMPLE -DLV_CONF_INCLUDE_SIMPLE -DLV_KCONFIG_IGNORE \
              -D_WASI_EMULATED_SIGNAL -Wno-unused-command-line-argument

web/weather-besties.wasm: $(WEB_OBJ)
	$(WCC) -O2 -mexec-model=reactor -Wl,--strip-all -Wl,-z,stack-size=262144 -o $@ $^ -lm

build-web/root/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(WCC) $(WEB_CFLAGS) -c $< -o $@

build-web/%.o: %.c
	@mkdir -p $(dir $@)
	@$(WCC) $(WEB_CFLAGS) -c $< -o $@

web: web/weather-besties.wasm
	python3 web/build_page.py

-include $(WEB_OBJ:.o=.d)
