# Lambda calculus scanner + parser.
# Design notes: docs/LEX_DESIGN.md, docs/YACC_DESIGN.md

BISON  ?= bison
FLEX   ?= flex
CC     ?= cc
CFLAGS ?= -std=c11 -O2
WARN   := -Wall -Wextra -Wno-unused-function -Wno-sign-compare

SRC    := src
BUILD  := build
BIN    := $(BUILD)/lc

GEN    := $(BUILD)/parser.tab.c $(BUILD)/lex.yy.c
OWN    := $(SRC)/ast.c $(SRC)/env.c $(SRC)/eval.c $(SRC)/main.c

.PHONY: all check conflicts clean cpp check-cpp check-all compare
all: $(BIN)

# The second implementation: hand-written scanner, recursive descent, C++.
cpp:
	@$(MAKE) -C cpp

check-cpp:
	@$(MAKE) -C cpp check

# Both implementations against the same suite.
check-all: check check-cpp compare

# Prove the two implementations agree on the language.
compare: $(BIN)
	@$(MAKE) -s -C cpp
	@tests/compare.sh $(BIN) cpp/$(BUILD)/lc

$(BUILD):
	@mkdir -p $(BUILD)

# -d emits parser.tab.h, the token-number contract the scanner includes.
# -v emits parser.output, the only place a grammar conflict explains itself.
$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: $(SRC)/parser.y | $(BUILD)
	$(BISON) -d -v -o $(BUILD)/parser.tab.c $(SRC)/parser.y

$(BUILD)/lex.yy.c: $(SRC)/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o $@ $(SRC)/lexer.l

$(BIN): $(GEN) $(OWN) $(SRC)/ast.h $(SRC)/env.h $(SRC)/eval.h
	$(CC) $(CFLAGS) $(WARN) -I$(SRC) -I$(BUILD) -o $@ $(GEN) $(OWN)

# The grammar is conflict-free by construction (YACC_DESIGN 2); any conflict
# is a real ambiguity, not noise.  Fails the build if one appears.
conflicts: $(BUILD)/parser.tab.c
	@if grep -q 'conflicts:' $(BUILD)/parser.output; then \
	    echo "grammar conflicts introduced:"; \
	    grep 'conflicts:' $(BUILD)/parser.output; \
	    echo "see $(BUILD)/parser.output"; exit 1; \
	else echo "0 conflicts"; fi

check: $(BIN) conflicts
	@tests/run.sh $(BIN)

clean:
	rm -rf $(BUILD)
	@$(MAKE) -C cpp clean
