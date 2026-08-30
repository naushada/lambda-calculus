# Two Front Ends: flex/bison vs. hand-written C++

This repository implements the same language twice. Design notes for each:

| | scanner | parser |
|---|---|---|
| flex/bison, C | [LEX_DESIGN.md](LEX_DESIGN.md) | [YACC_DESIGN.md](YACC_DESIGN.md) |
| hand-written, C++ | [CPP_SCANNER_DESIGN.md](CPP_SCANNER_DESIGN.md) | [CPP_PARSER_DESIGN.md](CPP_PARSER_DESIGN.md) |

This document is the comparison between them.

| | `src/` | `cpp/src/` |
|---|---|---|
| Scanner | `lexer.l` — flex | `lexer.cpp` — hand-written over `std::istream` |
| Parser | `parser.y` — bison LALR | `parser.cpp` — recursive descent |
| AST | tagged struct, `malloc`/`free` | `std::variant`, `unique_ptr` |
| Language | C | C++17 |
| Hand-written lines | 904 | 1000 |
| Generated lines | ~3500 | 0 |
| Build needs | flex, bison, cc | c++ only |

Both binaries take the same flags and produce the same output.
`make compare` runs 36 differential checks across every test file and flag
combination and asserts they agree — see §7.

The interesting part is not that both work. It is *where* the difficulty moved.

---

## 1. The λ problem: where the generator hurts

U+03BB is the two-byte UTF-8 sequence `0xCE 0xBB`, and flex matches **bytes**
with no notion of a code point. A negated class like `[^ \t\n.()]` therefore
matches `0xCE` and `0xBB` happily, so on input `λx` the `NAME` rule matches
three bytes while the λ literal matches two — and **longest match wins**. The
binder silently disappears into `NAME("λx")`.

Excluding those bytes is not the fix either: `0xCE` is the lead byte of *every*
Greek letter, so that outlaws `μ`, `π`, `Ω` as identifiers. What the flex
version needs is a class that admits `0xCE` only when what follows is not
`0xBB`:

```lex
CEOK      \xce[\x80-\xba\xbc-\xbf]
NOTCE     [^ \t\r\f\v\n.()\\=\xce]
NOTCE0    [^0-9 \t\r\f\v\n.()\\=\xce]
IDSTART   {NOTCE0}|{CEOK}
IDCHAR    {NOTCE}|{CEOK}
NAME      {IDSTART}{IDCHAR}*
```

Correct, and the range `[\x80-\xba\xbc-\xbf]` is load-bearing: it is exactly
"a UTF-8 continuation byte that is not `0xBB`", and relaxing it to `[^\xbb]`
lets a truncated `0xCE` swallow the following newline into a name — which
breaks the line counter. That bug was written, shipped and then caught by a
test during development. Roughly 40 lines of LEX_DESIGN §2.3 exist to explain
this.

The same rule with one character of lookahead:

```cpp
if (c == LAMBDA_LEAD && peek() == LAMBDA_TAIL) { get(); return Tok::Lambda; }
```

**This is the single strongest argument for hand-writing this scanner.** The
hardest thing in the flex version is an artifact of the tool, not of the
language.

---

## 2. What hand-writing costs instead

It is not free. Three things the generator handled silently now need care.

### 2.1 `char` is signed

```
plain char '\xCE' == 0xCE ?  NO        (clang warns: comparison always false)
as unsigned char:            206
```

`std::istream::get()` returns `int_type` for exactly this reason. Assign it to
a `char` before testing and the λ branch becomes dead code. Every byte in
`lexer.cpp` stays an `int`, and the one place a byte is stored uses an explicit
`static_cast<char>`. The flex byte-classes hid this class of bug entirely.

### 2.2 One character of lookahead is not enough

The binder must delimit even mid-word: `x` λ `y` is three tokens, so
`(xλy.y)` and `(x λy.y)` must parse identically. Deciding that while
accumulating a name means looking one byte *past* the `0xCE`, which is two
characters of lookahead.

Rather than widen the buffer, the scanner emits the name and stashes the
already-recognised `Lambda` in `pending_`, handing it out on the next call.
One token of pushback, not two characters of it.

### 2.3 `putback` is the wrong tool

`std::istream::putback` is guaranteed for only a single character and can fail
depending on stream state. `Lexer` keeps its own `std::optional<int> held_`
instead, so `get`/`peek`/`unget` are total functions the scanner fully
controls. `peek()` covers ordinary lookahead; `unget()` is used only on the
truncated-UTF-8 error path.

---

## 3. The parser: bison was optional all along

Because application is explicitly bracketed —
`'(' <function> <argument> ')'` — the grammar is **LL(1)**: every alternative
begins with a distinct token (`NAME`, `LAMBDA`, `LPAREN`) and every expression
is self-delimiting. That is the same property that let the bison grammar run at
zero conflicts without a single precedence declaration.

An LL(1) grammar needs no parser generator. `parser.cpp` is one function per
production and a single token of lookahead:

```cpp
case Tok::Lambda:
    advance();
    if (cur_.kind != Tok::Name) fail("expected a name after the binder");
    param = std::move(cur_.text);  advance();
    if (cur_.kind != Tok::Dot)  fail("expected '.' after the bound name");
    advance();
    return mk_abs(std::move(param), expression());
```

The one place two tokens are needed is telling a definition from an
expression: `NAME` followed by `EQ` starts a definition, anything else means
the name was an expression. The parser holds `cur_` and `ahead_` for that.

**Had application stayed conventional juxtaposition (`f x y`), this section
would read very differently.** That grammar is ambiguous, needs the
left-associativity and greedy-body conventions resolved, and a first
stratification attempt measured 3 shift/reduce conflicts (YACC_DESIGN §8).
Recursive descent would then need explicit precedence climbing. The bracketing
decision in the spec is what made the generator dispensable.

---

## 4. Memory: the least debatable win

The C version hand-writes `copy_node`/`free_node`, a `StrSet`, and `strdup`/
`free` ownership passed through `yylval` — with `%destructor` clauses to
reclaim values popped during error recovery. During development that setup
produced an uninitialised-`$$` double-free, caught only because bison warned
`unset value: $$`.

The C++ version deletes all of it: `unique_ptr` children, `std::string` names,
`std::set<std::string>` for free variables. The evaluator allocates a fresh
tree per reduction step, so this is exactly where ownership bugs live.

Both are clean under `leaks --atExit`; only one needed the audit.

---

## 5. Type safety in the AST

```c
/* C: children reused by convention, enforced by a comment */
typedef struct Node {
    NodeKind kind;
    char *name;              /* VAR: the variable; ABS: the bound name    */
    struct Node *l, *r;      /* ABS: l = body;  APP: l = rator, r = rand  */
} Node;
```

```cpp
// C++: each alternative names its own fields
struct Var { std::string name; };
struct Abs { std::string param; TermPtr body; };
struct App { TermPtr fn, arg; };
struct Term { std::variant<Var, Abs, App> node; };
```

Reading an `Abs` as though it were an `App` is a compile error in one and a
comment violation in the other.

---

## 6. Diagnostics: what recursive descent buys

Bison knows a parse failed; it does not know what you meant. A hand-written
parser is *at* the failure with full context:

```
flex/bison            hand-written
------------------    -------------------------------------------
line 1: syntax error  line 1: syntax error: trailing input after the expression
line 3: syntax error  line 3: syntax error: expected ')' to close the application
line 4: syntax error  line 4: syntax error: expected an expression
line 5: syntax error  line 5: syntax error: expected a name after the binder
line 6: syntax error  line 6: syntax error: expected '.' after the bound name
```

Both flag the same lines; only one says what was wrong. `%error-verbose` would
narrow but not close the gap, and its phrasing is the generator's, not yours.

*(The bracket-arity message — "an application needs two expressions" — is the
exception: it is good in both, because in the bison version it is a dedicated
error production rather than the generic handler.)*

---

## 7. The differential test

Two implementations of one language are only useful if they agree. `make
compare` runs both binaries over every test file under `-p`, `-s 200`,
`-a -s 200`, `-t -s 40`, `-e -s 200` and `-e -a -s 200`, plus a UTF-8
edge-case file, and checks:

- **stdout** — byte for byte.
- **exit status** — must match.
- **stderr** — *structurally*: the same input lines flagged, the same number of
  times. Wording is deliberately not compared, because §6 is a real advantage
  and forcing byte-equality there would mean making the C++ version worse.

36 comparisons, all agreeing. Writing it immediately paid: it caught that the C
version's diagnostics jumped ahead of its results in a merged stream, because
`stdout` is block-buffered when piped while `stderr` is not. `std::cerr` is
tied to `std::cout` and flushes it, so the C++ version never had the problem.
Fixed with `fflush(stdout)` before each diagnostic.

---

## 8. When to use which

Hand-writing won here for reasons that are specific, not universal:

- the token set is tiny (7 kinds, one scanner state, no string literals or
  nested comments);
- the grammar is LL(1) by construction;
- the input is UTF-8, which byte-oriented flex handles badly.

Change any one and the answer changes. A language with string literals, nested
comments and a genuinely ambiguous expression grammar is where a generator
earns its keep: the `.l` and `.y` files stay a *specification*, conflicts are
reported rather than silently mis-resolved, and `bison -v` explains itself.
That is why both implementations are kept here rather than one being deleted.
