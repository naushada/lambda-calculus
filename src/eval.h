/* eval.h — beta reduction for the lambda calculus.
 *
 * Reduction strategies (both reduce under a binder, so both compute a full
 * normal form rather than stopping at weak head normal form):
 *
 *   NORMAL_ORDER      leftmost-outermost.  Contracts a redex before reducing
 *                     its argument, so it finds a normal form whenever one
 *                     exists -- including when an argument diverges but is
 *                     never used.
 *   APPLICATIVE_ORDER leftmost-innermost.  Reduces arguments first.  Cheaper
 *                     when an argument is used more than once, but diverges
 *                     on some terms that do have a normal form.
 *
 * Reduction is not guaranteed to terminate -- (λx.(x x) λx.(x x)) is the
 * standard counterexample -- so every entry point takes a step limit.
 *
 * Eta reduction -- λx.(M x) -> M when x is not free in M -- is optional, and
 * off by default.  It is a different normal form, not an optimisation: with
 * it, λx.λy.(x y) collapses to λx.x, and λy1.(y y1) to y.  Leaving it off
 * keeps beta normal forms intact (the second of those is what makes a
 * capture-avoiding substitution visible), so the caller opts in.
 */
#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

typedef enum { NORMAL_ORDER, APPLICATIVE_ORDER } Strategy;

typedef enum {
    EVAL_NORMAL_FORM,   /* no redex remains          */
    EVAL_LIMIT          /* step limit reached first  */
} EvalStatus;

Node *copy_node(const Node *n);

/* Capture-avoiding substitution: body[name := value].  Returns a new tree;
 * the inputs are untouched.  Alpha-renames a binder when a naive substitution
 * would capture a free variable of `value`. */
Node *substitute(const Node *body, const char *name, const Node *value);

/* One reduction step, or NULL if `n` is already in normal form. */
Node *reduce_step(const Node *n, Strategy s, int eta);

/* Reduce to a normal form or until `limit` steps have been taken.  Consumes
 * `term` (frees it) and returns the result.  `*steps` and `*status` report
 * what happened; `trace` prints each intermediate term to stdout. */
Node *reduce(Node *term, Strategy s, int eta, long limit, int trace,
             long *steps, EvalStatus *status);

#endif /* EVAL_H */
