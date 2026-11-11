# THIS MAKEFILE IS AI GENERATED.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
BUILD    := build
TARGET   := $(BUILD)/lanes

SRCS := main.cpp $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

LIB_OBJS     := $(SRCS:main.cpp=)
LIB_OBJS     := $(patsubst %.cpp,$(BUILD)/%.o,$(filter %.cpp,$(LIB_OBJS)))
TEST_SRCS    := tests/run_tests.cpp tests/lexer/run_lexer_test.cpp tests/parser/run_parser_test.cpp
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
