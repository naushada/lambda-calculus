/* eval.h — beta reduction.
 *
 *   Normal      leftmost-outermost.  Contracts a redex before reducing its
 *               argument, so it finds a normal form whenever one exists --
 *               including when an argument diverges but is never used.
 *   Applicative leftmost-innermost.  Reduces arguments first.  Cheaper when
 *               an argument is used more than once, but diverges on some
 *               terms that do have a normal form.
 *
 * Both reduce under a binder, so both compute a full normal form.  Reduction
 * need not terminate, so every entry point takes a step limit.
 */
#ifndef EVAL_H
#define EVAL_H

#include <iosfwd>
#include <string>

#include "ast.h"

enum class Strategy { Normal, Applicative };
enum class Status   { NormalForm, Limit };

/* body[name := value], capture-avoiding.  Alpha-renames a binder when a naive
 * substitution would capture a free variable of `value`. */
TermPtr substitute(const Term &body, const std::string &name,
                   const Term &value);

/* One reduction step, or nullptr if `t` is already in normal form. */
TermPtr reduce_step(const Term &t, Strategy s);

TermPtr reduce(TermPtr t, Strategy s, long limit, bool trace,
               long &steps, Status &status, std::ostream &os);

#endif /* EVAL_H */
