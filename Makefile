# Configuración
CC = gcc
CFLAGS = -Iinclude -std=c11 -Wall -Wextra -pthread
DEBUG_FLAGS = -DMEMORY_DEBUG -g
RELEASE_FLAGS = -O2

SRC_DIR = src
BUILD_DIR = build
EXAMPLES_DIR = examples

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
LIBRARY = $(BUILD_DIR)/libmemory_manager.a

EXAMPLES = $(wildcard $(EXAMPLES_DIR)/*.c)
EXECUTABLES = $(EXAMPLES:$(EXAMPLES_DIR)/%.c=$(BUILD_DIR)/%)

# Targets principales
all: release

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(LIBRARY) $(EXECUTABLES)

release: CFLAGS += $(RELEASE_FLAGS)
release: $(LIBRARY) $(EXECUTABLES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

$(BUILD_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIBRARY)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lmemory_manager -o $@

# Utilidades
clean:
	rm -rf $(BUILD_DIR)

test: debug
	$(BUILD_DIR)/basic_usage

install: release
	cp $(LIBRARY) /usr/local/lib/
	cp -r include/* /usr/local/include/

.PHONY: all debug release clean test install
