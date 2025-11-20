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

build/test_detailed_analysis: examples/test_detailed_analysis.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/test_detailed_analysis.c -Lbuild -lmemory_manager -o build/test_detailed_analysis

build/test_next_fit_debug: examples/test_next_fit_debug.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/test_next_fit_debug.c -Lbuild -lmemory_manager -o build/test_next_fit_debug

build/benchmark_simple: examples/benchmark_simple.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/benchmark_simple.c -Lbuild -lmemory_manager -o build/benchmark_simple

build/benchmark_strategies: examples/benchmark_strategies.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/benchmark_strategies.c -Lbuild -lmemory_manager -o build/benchmark_strategies

build/benchmark_concurrent: examples/benchmark_concurrent.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/benchmark_concurrent.c -Lbuild -lmemory_manager -o build/benchmark_concurrent -lpthread

build/list: examples/list.c build/libmemory_manager.a
	$(CC) $(CFLAGS) examples/list.c -Lbuild -lmemory_manager -o build/list

benchmark: build/benchmark_simple build/benchmark_strategies build/benchmark_concurrent build/list
	@echo "=== EJECUTANDO BENCHMARKS ==="
	@echo "1. Benchmark Simple..."
	@./build/benchmark_simple
	@echo ""
	@echo "2. Benchmark Estrategias..."
	@./build/benchmark_strategies
	@echo ""
	@echo "3. Benchmark Concurrente..."
	@./build/benchmark_concurrent

benchmark_all: benchmark

test_strategies: build/test_detailed_analysis
	./build/test_detailed_analysis

test_next_fit: build/test_next_fit_debug
	./build/test_next_fit_debug

test_all: build/basic_usage build/test_size_audit build/test_detailed_analysis build/test_next_fit_debug
	./build/basic_usage
	./build/test_size_audit
	./build/test_detailed_analysis
	./build/test_next_fit_debug

.PHONY: all debug release clean test install
