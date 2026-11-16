# THIS MAKEFILE IS AI GENERATED.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
BUILD    := build
TARGET   := $(BUILD)/lanes

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

LIB_OBJS     := $(patsubst %.cpp,$(BUILD)/%.o,$(filter-out src/main.cpp,$(SRCS)))
TEST_SRCS    := tests/run_tests.cpp tests/lexer/run_lexer_test.cpp tests/parser/run_parser_test.cpp  tests/codegen/run_wat_test.cpp tests/codegen/run_codegen_test.cpp tests/semantic/run_symbol_table_test.cpp tests/semantic/run_control_flow_test.cpp tests/semantic/run_type_annotation_test.cpp tests/semantic/run_recursion_test.cpp tests/semantic/run_builtin_test.cpp tests/semantic/run_field_operation_test.cpp tests/semantic/run_ruleset_call_test.cpp tests/semantic/run_reduction_test.cpp tests/semantic/run_math_builtin_test.cpp tests/semantic/run_logical_test.cpp tests/semantic/run_ruleset_contract_test.cpp tests/semantic/run_for_loop_test.cpp tests/exec/run_exec_test.cpp
TEST_TARGET  := $(BUILD)/run_tests

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# usage: make run program.lanes  (extra goals after `run` are passed to the compiler)
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

# usage: make demo  (then open demo/benchmarks.html in a browser, no server needed)
# the demo sources are in demo/src: the page template in html/ and the LANES
# programs in lanes/. the finished page is written to demo/ itself (needs
# wat2wasm and node)
DEMO_HTML  := demo/src/html
DEMO_LANES := demo/src/lanes

BENCHMARKS := blur reaction boids voronoi matmul regression

demo: demo/benchmarks.html

# the benchmark programs. the compiler writes the .wasm next to its input, so it is
# moved into the build folder afterwards
$(BUILD)/demo/bench/%.wasm: $(DEMO_LANES)/%.lanes $(TARGET)
	@mkdir -p $(dir $@)
	./$(TARGET) $<
	mv $(DEMO_LANES)/$*.wasm $@

demo/benchmarks.html: $(DEMO_HTML)/benchmarks.template.html $(BENCHMARKS:%=$(BUILD)/demo/bench/%.wasm) $(BENCHMARKS:%=$(DEMO_LANES)/%.lanes) tools/embed_wasm.mjs
	node tools/embed_wasm.mjs $(DEMO_HTML)/benchmarks.template.html $@ \
		$(foreach name,$(BENCHMARKS),$(name).wasm=$(BUILD)/demo/bench/$(name).wasm $(name).lanes=$(DEMO_LANES)/$(name).lanes)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all run test clean install demo
