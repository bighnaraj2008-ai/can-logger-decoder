# CAN Logger & Decoder - Day 1 Modular Architecture Makefile
#
# GNU99 standard, strict compiler warnings, cross-platform build support.

ifeq ($(origin CC),default)
CC = gcc
endif
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude
LDFLAGS = -lm

BUILD_DIR = build
TARGET = can_decoder

# Source files categorized by architectural layer
CORE_SRCS    = src/core/can_parser.c src/core/can_frame.c src/core/dlc_validator.c \
               src/core/can_router.c
HW_SRCS      = src/hardware/mcp2515_spi.c src/hardware/mcp2515_init.c \
               src/hardware/mcp2515_interrupt.c src/hardware/mcp2515_filters.c
NET_SRCS     = src/network/vcan_socket.c src/network/vcan_listener.c
STATE_SRCS   = src/state/error_machine.c src/state/bus_off_recovery.c
UTIL_SRCS    = src/utils/logger.c src/utils/crc_check.c src/utils/metrics.c
DBC_SRCS     = src/dbc_decoder.c
APP_SRCS     = src/main.c

LIB_SRCS = $(CORE_SRCS) $(HW_SRCS) $(NET_SRCS) $(STATE_SRCS) $(UTIL_SRCS) $(DBC_SRCS)
LIB_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(LIB_SRCS)))
APP_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(APP_SRCS)))

TEST_BINS = $(BUILD_DIR)/test_parser $(BUILD_DIR)/test_state_machine \
            $(BUILD_DIR)/test_vcan $(BUILD_DIR)/test_filters \
            $(BUILD_DIR)/test_metrics $(BUILD_DIR)/test_router

vpath %.c src src/core src/hardware src/network src/state src/utils tests

.PHONY: all clean test run prepare

all: prepare $(TARGET)

prepare:
ifeq ($(OS),Windows_NT)
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
else
	@mkdir -p "$(BUILD_DIR)"
endif

$(BUILD_DIR)/%.o: src/core/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/hardware/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/network/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/state/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/utils/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | prepare
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(APP_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_%: tests/test_%.c $(LIB_OBJS) | prepare
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(LDFLAGS)

test: prepare $(TEST_BINS)
ifeq ($(OS),Windows_NT)
	build\test_parser.exe
	build\test_state_machine.exe
	build\test_vcan.exe
	build\test_filters.exe
	build\test_metrics.exe
	build\test_router.exe
else
	@for b in $(TEST_BINS); do ./$$b || exit 1; done
endif

run: $(TARGET)
ifeq ($(OS),Windows_NT)
	$(TARGET) -d sample.dbc -c 20
else
	./$(TARGET) -d sample.dbc -c 20
endif

clean:
ifeq ($(OS),Windows_NT)
	@-del /Q /F $(TARGET).exe $(addsuffix .exe,$(TEST_BINS)) $(BUILD_DIR)\*.o 2>nul
	@-if exist "$(BUILD_DIR)" rmdir /Q /S "$(BUILD_DIR)" 2>nul
else
	@-rm -f $(TARGET) $(TEST_BINS) $(BUILD_DIR)/*.o
	@-rm -rf "$(BUILD_DIR)" 2>/dev/null || true
endif

distclean: clean
	rm -rf $(BUILD_DIR) release_* *.tar.gz