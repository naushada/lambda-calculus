#include "ast.h"

#include <ostream>

TermPtr mk_var(std::string name)
{
    return std::make_unique<Term>(Var{std::move(name)});
}

TermPtr mk_abs(std::string param, TermPtr body)
{
    return std::make_unique<Term>(Abs{std::move(param), std::move(body)});
}

TermPtr mk_app(TermPtr fn, TermPtr arg)
{
    return std::make_unique<Term>(App{std::move(fn), std::move(arg)});
}

TermPtr clone(const Term &t)
{
    if (auto *v = std::get_if<Var>(&t.node))
        return mk_var(v->name);
    if (auto *a = std::get_if<Abs>(&t.node))
        return mk_abs(a->param, clone(*a->body));
    const auto &ap = std::get<App>(t.node);
    return mk_app(clone(*ap.fn), clone(*ap.arg));
}

void print(const Term &t, std::ostream &os)
{
    if (auto *v = std::get_if<Var>(&t.node)) {
        os << v->name;
    } else if (auto *a = std::get_if<Abs>(&t.node)) {
        os << "(\xce\xbb" << a->param << '.';   /* U+03BB */
        print(*a->body, os);
        os << ')';
    } else {
        const auto &ap = std::get<App>(t.node);
        os << '(';
        print(*ap.fn, os);
        os << ' ';
        print(*ap.arg, os);
        os << ')';
    }
}
