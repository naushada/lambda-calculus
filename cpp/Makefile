# Lambda calculus: hand-written scanner and recursive-descent parser in C++.
# The flex/bison implementation of the same language lives in ../src.
# Design notes: ../docs/CPP_SCANNER_DESIGN.md, ../docs/CPP_PARSER_DESIGN.md
# Comparison with the flex/bison build: ../docs/COMPARISON.md

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2
WARN     := -Wall -Wextra

SRC   := src
BUILD := build
BIN   := $(BUILD)/lc

SRCS  := $(SRC)/ast.cpp $(SRC)/lexer.cpp $(SRC)/parser.cpp \
         $(SRC)/eval.cpp $(SRC)/env.cpp $(SRC)/main.cpp
HDRS  := $(SRC)/ast.h $(SRC)/lexer.h $(SRC)/parser.h \
         $(SRC)/eval.h $(SRC)/env.h

.PHONY: all check clean
all: $(BIN)

$(BUILD):
	@mkdir -p $(BUILD)

$(BIN): $(SRCS) $(HDRS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARN) -I$(SRC) -o $@ $(SRCS)

# The same suite the flex/bison build runs: identical language, identical
# output, so the tests are the proof that the two implementations agree.
check: $(BIN)
	@cd .. && tests/run.sh cpp/$(BIN)

clean:
	rm -rf $(BUILD)
