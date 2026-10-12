CC ?= gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

TARGET = can_decoder
TEST_TARGET = test_suite

COMMON_SRCS = $(SRC_DIR)/can_parser.c $(SRC_DIR)/dbc_decoder.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(MAIN_SRC:.c=.o)

TEST_SRC = $(SRC_DIR)/test_decoder.c
TEST_OBJ = $(TEST_SRC:.c=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJ) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) -d sample.dbc -c 20

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(SRC_DIR)/*.o *.o can_log.csv
