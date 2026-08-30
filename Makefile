CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lm
TARGET_NAME = nc

BUILD_DIR = build
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

SRCS = main.c vec.c compiler.c operations.c out.c
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
