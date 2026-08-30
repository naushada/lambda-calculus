# lambda-calculus

A scanner (`flex`) and parser (`bison`/`yacc`) for the lambda calculus, written
with the design worked out first and the conflict counts measured rather than
assumed.

```
expression   ::= name
               | function
               | application

name         ::= sequence of non-blank characters,
                 may end with digits, must not start with a digit
function     ::= λ <name> . <body>
body         ::= expression
application  ::= '(' <function expression> <argument expression> ')'
```

## Build

```sh
make          # build/lc
make check    # 0-conflict check + 18 parse vectors + error-recovery test
```

Builds with the stock macOS toolchain (bison 2.3) as well as modern bison/flex.

## Use

One expression per line. By default each term is reduced to a normal form and
printed fully parenthesised. `\` is accepted wherever `λ` is, for keyboards that
cannot produce it; `#` starts a comment.

```sh
$ echo '(λf.λx.(f (f x)) λy.y)' | ./build/lc      # Church 2 applied to identity
(λx.x)

$ echo '(λf.λx.(f (f x)) λy.y)' | ./build/lc -p   # parse only, no reduction
((λf.(λx.(f (f x)))) (λy.y))

$ echo '(λf.λx.(f (f x)) λy.y)' | ./build/lc -t   # trace every step
    0  ((λf.(λx.(f (f x)))) (λy.y))
    1  (λx.((λy.y) ((λy.y) x)))
    2  (λx.((λy.y) x))
    3  (λx.x)
(λx.x)
```

```
-p     parse only; print the AST without reducing
-t     trace every reduction step
-a     applicative order (default: normal order)
-s N   step limit before giving up (default 10000)
```

### Reduction

Substitution is capture-avoiding: it alpha-renames a binder when a naive
substitution would capture a free variable.

```sh
$ echo '(λx.λy.(x y) y)' | ./build/lc
(λy1.(y y1))            # not (λy.(y y)) — the free y must not be captured
```

Both strategies reduce under a binder, so both compute a full normal form.
They are not interchangeable: normal order (leftmost-outermost) finds a normal
form whenever one exists, while applicative order (leftmost-innermost) reduces
arguments first and can diverge on a term that has one.

```sh
$ echo '(λx.y (λx.(x x) λx.(x x)))' | ./build/lc       # argument is unused
y
$ echo '(λx.y (λx.(x x) λx.(x x)))' | ./build/lc -a     # ...but evaluated anyway
((λx.y) ((λx.(x x)) (λx.(x x))))   [no normal form after 10000 steps]
```

Reduction need not terminate, so `-s` bounds it; hitting the limit prints the
term reached and exits non-zero.

## Two things that surprise people

**Brackets are application syntax, not grouping.** Every `(` must contain
exactly two expressions, so `(λx.x)` is a syntax error — the parser says so
explicitly. See [YACC_DESIGN §7](docs/YACC_DESIGN.md) for the one-line change
that turns brackets into grouping as well.

**`(λx.x y)` here means `((λx.x) y)`**, not `(λx.(x y))` as conventional
λ-calculus notation would have it. Because application is explicitly bracketed,
the "body extends as far right as possible" convention does not apply. Write
`λx.(x y)` for the other tree.

## Design notes

- [docs/LEX_DESIGN.md](docs/LEX_DESIGN.md) — tokenisation. The interesting part
  is §2.3: flex matches *bytes*, and λ is the two-byte sequence `0xCE 0xBB`, so
  a naive `NAME` pattern silently swallows the binder. The fix keeps every other
  Greek letter legal in identifiers.
- [docs/YACC_DESIGN.md](docs/YACC_DESIGN.md) — the grammar. Three productions,
  no precedence declarations, **0 shift/reduce and 0 reduce/reduce conflicts**.
  §8 keeps the analysis for the conventional juxtaposition syntax (`f x y`),
  which is not conflict-free without care.

## Status

Scanner, parser and evaluator. Not implemented: `let` bindings for named terms,
eta reduction, and an interactive REPL (input is read as a plain stream).
