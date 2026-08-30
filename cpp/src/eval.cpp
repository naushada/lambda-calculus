#include "eval.h"

#include <iomanip>
#include <ostream>
#include <set>

namespace {

using NameSet = std::set<std::string>;

void free_vars(const Term &t, NameSet &out)
{
    if (auto *v = std::get_if<Var>(&t.node)) {
        out.insert(v->name);
    } else if (auto *a = std::get_if<Abs>(&t.node)) {
        NameSet inner;
        free_vars(*a->body, inner);
        inner.erase(a->param);            /* the binder is not free */
        out.insert(inner.begin(), inner.end());
    } else {
        const auto &ap = std::get<App>(t.node);
        free_vars(*ap.fn, out);
        free_vars(*ap.arg, out);
    }
}

/* A name like `x` that appears in neither set.  Tries x1, x2, ... which stay
 * legal identifiers: a name may end with digits. */
std::string fresh_name(const std::string &base, const NameSet &a,
                       const NameSet &b)
{
    for (unsigned i = 1;; i++) {
        std::string cand = base + std::to_string(i);
        if (!a.count(cand) && !b.count(cand))
            return cand;
    }
}

/* Contract a redex: (\x.M N) -> M[x := N] */
TermPtr beta(const Abs &fn, const Term &arg)
{
    return substitute(*fn.body, fn.param, arg);
}

} // namespace

TermPtr substitute(const Term &body, const std::string &name,
                   const Term &value)
{
    if (auto *v = std::get_if<Var>(&body.node))
        return v->name == name ? clone(value) : clone(body);

    if (auto *ap = std::get_if<App>(&body.node))
        return mk_app(substitute(*ap->fn, name, value),
                      substitute(*ap->arg, name, value));

    const auto &abs = std::get<Abs>(body.node);

    /* The binder shadows `name`: nothing inside is free for it. */
    if (abs.param == name)
        return clone(body);

    NameSet fv_value;
    free_vars(value, fv_value);

    /* The binder does not occur free in `value`, so no capture is possible. */
    if (!fv_value.count(abs.param))
        return mk_abs(abs.param, substitute(*abs.body, name, value));

    /* Capture would occur: alpha-rename the binder first.  The new name must
     * avoid the free variables of `value` (what is being inserted) and of the
     * body (what is already there). */
    NameSet fv_body;
    free_vars(*abs.body, fv_body);

    std::string z       = fresh_name(abs.param, fv_value, fv_body);
    TermPtr     zvar    = mk_var(z);
    TermPtr     renamed = substitute(*abs.body, abs.param, *zvar);

    return mk_abs(std::move(z), substitute(*renamed, name, value));
}

TermPtr reduce_step(const Term &t, Strategy s)
{
    if (auto *abs = std::get_if<Abs>(&t.node)) {
        TermPtr step = reduce_step(*abs->body, s);
        return step ? mk_abs(abs->param, std::move(step)) : nullptr;
    }

    if (auto *ap = std::get_if<App>(&t.node)) {
        /* Outermost first: contract before touching the argument. */
        if (s == Strategy::Normal)
            if (auto *fn = std::get_if<Abs>(&ap->fn->node))
                return beta(*fn, *ap->arg);

        /* Leftmost: the function position before the argument position. */
        if (TermPtr step = reduce_step(*ap->fn, s))
            return mk_app(std::move(step), clone(*ap->arg));
        if (TermPtr step = reduce_step(*ap->arg, s))
            return mk_app(clone(*ap->fn), std::move(step));

        /* Innermost: both parts are in normal form, so contract now. */
        if (s == Strategy::Applicative)
            if (auto *fn = std::get_if<Abs>(&ap->fn->node))
                return beta(*fn, *ap->arg);
    }

    return nullptr;   /* a variable, or no redex anywhere below */
}

TermPtr reduce(TermPtr t, Strategy s, long limit, bool trace,
               long &steps, Status &status, std::ostream &os)
{
    long n = 0;

    for (; n < limit; n++) {
        TermPtr next = reduce_step(*t, s);
        if (!next) {
            steps  = n;
            status = Status::NormalForm;
            return t;
        }
        t = std::move(next);
        if (trace) {
            os << "  " << std::setw(3) << (n + 1) << "  ";
            print(*t, os);
            os << '\n';
        }
    }

    steps  = n;
    status = Status::Limit;
    return t;
}
