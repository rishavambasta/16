CC ?= gcc

ifeq ($(OS),Windows_NT)
    TARGET = game16.exe
    BUILD_DIR = build_win
    LDFLAGS ?=
    CFLAGS ?= -Wall -Wextra -O3 -std=c11
    RM = del /Q /F
else
    ifneq ($(findstring mingw,$(CC)),)
        TARGET = game16.exe
        BUILD_DIR = build_win
        LDFLAGS ?=
        CFLAGS ?= -Wall -Wextra -O3 -std=c11
        RM = rm -rf
    else
        TARGET = game16
        BUILD_DIR = build_linux
        LDFLAGS ?= -lncurses
        CFLAGS ?= -Wall -Wextra -O3 -std=c11 -D_POSIX_C_SOURCE=200809L
        RM = rm -rf
    endif
endif

SRCS = main.c board.c solver.c ui.c
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

.PHONY: all clean test demo windows

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TARGET)
ifeq ($(TARGET),game16.exe)
	wine ./$(TARGET) --test
else
	./$(TARGET) --test
endif

demo: $(TARGET)
ifeq ($(TARGET),game16.exe)
	wine ./$(TARGET) --demo 20 0
else
	./$(TARGET) --demo 20 0
endif

windows:
	$(MAKE) CC=x86_64-w64-mingw32-gcc

clean:
	$(RM) build_linux build_win game16 game16.exe
