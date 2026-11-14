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

run: $(TARGET)
	./$(TARGET)

$(TEST_TARGET): $(TEST_SRCS) $(LIB_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all run test clean
