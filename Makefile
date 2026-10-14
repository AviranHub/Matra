CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS = -Isrc/common -Itests/unit
BUILD_DIR = build
PROJECT_SOURCES = $(wildcard src/common/*.c src/source/*.c src/lexer/*.c src/ast/*.c src/parser/*.c src/sema/*.c)
PROJECT_HEADERS = $(wildcard src/common/*.h src/source/*.h src/lexer/*.h src/ast/*.h src/parser/*.h src/sema/*.h)
TEST_SOURCES = $(wildcard tests/unit/test_*.c)
TEST_HEADERS = $(wildcard tests/unit/*.h)
TEST_MAIN = tests/unit/main.c

ifeq ($(OS),Windows_NT)
TEST_BINARY = $(BUILD_DIR)/unit_tests.exe
CREATE_BUILD_DIR = if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
DELETE_BUILD_DIR = if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
else
TEST_BINARY = $(BUILD_DIR)/unit_tests
CREATE_BUILD_DIR = mkdir -p $(BUILD_DIR)
DELETE_BUILD_DIR = rm -rf $(BUILD_DIR)
endif

.PHONY: all test clean

all: $(TEST_BINARY)

$(TEST_BINARY): $(PROJECT_SOURCES) $(PROJECT_HEADERS) $(TEST_SOURCES) $(TEST_HEADERS) $(TEST_MAIN)
	$(CREATE_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROJECT_SOURCES) $(TEST_SOURCES) $(TEST_MAIN) -o $@

test: $(TEST_BINARY)
	$(TEST_BINARY)

clean:
	$(DELETE_BUILD_DIR)
