/* ast.h — abstract syntax tree.
 *
 * A Term is a variant of three node types.  Unlike a tagged struct with
 * reused child pointers, each alternative names its own fields, so `Abs`
 * cannot be read as though it were an `App` and the invariant is checked by
 * the compiler rather than by a comment.
 *
 * Ownership is unique_ptr throughout: a term owns its children, copying is
 * explicit via clone(), and there is nothing to free by hand.
 */
#ifndef AST_H
#define AST_H

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <variant>

struct Term;
using TermPtr = std::unique_ptr<Term>;

struct Var { std::string name; };
struct Abs { std::string param; TermPtr body; };
struct App { TermPtr fn, arg; };

struct Term {
    std::variant<Var, Abs, App> node;

    template <typename N>
    explicit Term(N n) : node(std::move(n)) {}
};

TermPtr mk_var(std::string name);
TermPtr mk_abs(std::string param, TermPtr body);
TermPtr mk_app(TermPtr fn, TermPtr arg);

TermPtr clone(const Term &t);

/* Fully parenthesised, matching the notation used throughout the docs. */
void print(const Term &t, std::ostream &os);

#endif /* AST_H */
