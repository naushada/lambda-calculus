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

```sh
$ echo 'λx.(x y)' | ./build/lc
(λx.(x y))

$ echo '(λf.λx.(f (f x)) λy.y)' | ./build/lc
((λf.(λx.(f (f x)))) (λy.y))
```

One expression per line; output is the fully parenthesised AST. `\` is accepted
wherever `λ` is, for keyboards that cannot produce it. `#` starts a comment.

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

Scanner and parser only — the output is an AST. Evaluation (capture-avoiding
substitution, β-reduction, a REPL) is not implemented; see YACC_DESIGN §10.
