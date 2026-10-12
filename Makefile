CC ?= gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude
LDFLAGS = -lm

SRC_DIR = src
INC_DIR = include

COMMON_SRCS = $(SRC_DIR)/can_parser.c $(SRC_DIR)/dbc_decoder.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(MAIN_SRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
