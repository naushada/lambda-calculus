#include "env.h"

#include <algorithm>

namespace {

/* Lambda binders in scope at the current point of the walk. */
struct Bound {
    const std::string *name;
    const Bound       *next;
};

bool is_bound(const Bound *b, const std::string &name)
{
    for (; b; b = b->next)
        if (*b->name == name)
            return true;
    return false;
}

} // namespace

const Env::Def *Env::find(const std::string &name) const
{
    auto it = std::find_if(defs_.begin(), defs_.end(),
                           [&](const Def &d) { return d.name == name; });
    return it == defs_.end() ? nullptr : &*it;
}

void Env::define(std::string name, TermPtr term)
{
    auto it = std::find_if(defs_.begin(), defs_.end(),
                           [&](const Def &d) { return d.name == name; });
    if (it != defs_.end())
        it->term = std::move(term);
    else
        defs_.push_back(Def{std::move(name), std::move(term)});
}

TermPtr Env::expand(const Term &t) const
{
    struct Walk {
        const Env *env;

        TermPtr go(const Term &t, const Bound *bound) const
        {
            if (auto *v = std::get_if<Var>(&t.node)) {
                /* A binder shadows a definition of the same name. */
                if (is_bound(bound, v->name))
                    return clone(t);
                if (const Def *d = env->find(v->name))
                    return clone(*d->term);
                return clone(t);
            }
            if (auto *a = std::get_if<Abs>(&t.node)) {
                Bound b{&a->param, bound};
                return mk_abs(a->param, go(*a->body, &b));
            }
            const auto &ap = std::get<App>(t.node);
            return mk_app(go(*ap.fn, bound), go(*ap.arg, bound));
        }
    };

    return Walk{this}.go(t, nullptr);
}
