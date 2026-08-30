/* env.h — top-level definitions:  name = expression
 *
 * A definition is expanded at the moment it is made: every free occurrence of
 * an already-defined name in the right-hand side is replaced by that name's
 * (already expanded) term before the binding is stored.  Two consequences:
 *
 *   - Lookup is never recursive, so expansion always terminates.
 *   - A definition cannot refer to itself, because its own name is not yet
 *     bound while its right-hand side is being expanded.  Recursion is
 *     available the usual way, through a fixed-point combinator.
 *
 * Expansion respects scope: only FREE occurrences are replaced, so a lambda
 * binder always shadows a definition of the same name.
 */
#ifndef ENV_H
#define ENV_H

#include <string>
#include <vector>

#include "ast.h"

class Env {
public:
    /* Rebinding a name replaces the previous definition; terms already built
     * from it are unaffected. */
    void define(std::string name, TermPtr term);

    TermPtr expand(const Term &t) const;

private:
    struct Def {
        std::string name;
        TermPtr     term;
    };

    const Def *find(const std::string &name) const;

    std::vector<Def> defs_;
};

#endif /* ENV_H */
