# THIS MAKEFILE IS AI GENERATED.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
BUILD    := build
TARGET   := $(BUILD)/lanes

SRCS := main.cpp $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

LIB_OBJS     := $(SRCS:main.cpp=)
LIB_OBJS     := $(patsubst %.cpp,$(BUILD)/%.o,$(filter %.cpp,$(LIB_OBJS)))
TEST_SRC     := tests/tokenizer/run_tokenizer_test.cpp
TEST_TARGET  := $(BUILD)/run_tokenizer_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

$(TEST_TARGET): $(TEST_SRC) $(LIB_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all run test clean
