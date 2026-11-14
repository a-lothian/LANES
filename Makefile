# THIS MAKEFILE IS AI GENERATED.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
BUILD    := build
TARGET   := $(BUILD)/lanes

SRCS := main.cpp $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

LIB_OBJS     := $(SRCS:main.cpp=)
LIB_OBJS     := $(patsubst %.cpp,$(BUILD)/%.o,$(filter %.cpp,$(LIB_OBJS)))
TEST_SRCS    := tests/run_tests.cpp tests/lexer/run_lexer_test.cpp tests/parser/run_parser_test.cpp  tests/codegen/run_wat_test.cpp tests/codegen/run_codegen_test.cpp tests/semantic/run_symbol_table_test.cpp tests/semantic/run_control_flow_test.cpp tests/semantic/run_type_annotation_test.cpp
TEST_TARGET  := $(BUILD)/run_tests

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# usage: make run input.txt  (extra goals after `run` are passed to the compiler)
RUN_ARGS := $(filter-out run,$(MAKECMDGOALS))

run: $(TARGET)
	./$(TARGET) $(RUN_ARGS)

# stop make treating the input file as a target to build
ifeq ($(firstword $(MAKECMDGOALS)),run)
.PHONY: $(RUN_ARGS)
$(RUN_ARGS):
	@:
endif

# usage: make install  (installs wat2wasm, which turns the .wat output into .wasm)
install:
	@if command -v wat2wasm >/dev/null 2>&1; then \
		echo "wat2wasm is already installed"; \
	elif command -v brew >/dev/null 2>&1; then \
		brew install wabt; \
	elif command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get install -y wabt; \
	else \
		echo "no supported package manager found: install wabt manually (https://github.com/WebAssembly/wabt)"; \
		exit 1; \
	fi

$(TEST_TARGET): $(TEST_SRCS) $(LIB_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all run test clean install
