/* main.cpp — driver: read lines, bind definitions, reduce expressions. */
#include <fstream>
#include <iostream>
#include <string>

#include "env.h"
#include "eval.h"
#include "lexer.h"
#include "parser.h"

namespace {

struct Options {
    Strategy    strategy   = Strategy::Normal;
    long        step_limit = 10000;
    bool        parse_only = false;
    bool        trace      = false;
    bool        eta        = false;
    std::string path;
};

void usage(const char *prog, int code)
{
    std::ostream &os = code ? std::cerr : std::cout;
    os << "usage: " << prog << " [-p] [-t] [-a] [-s N] [file]\n"
       << "  reads `expr` lines and `name = expr` definitions\n"
       << "  -p     parse only; print the AST without expanding or reducing\n"
       << "  -t     trace every reduction step\n"
       << "  -a     applicative order (default: normal order)\n"
       << "  -e     also apply eta reduction: lambda x.(M x) -> M\n"
       << "  -s N   step limit before giving up (default 10000)\n"
       << "  file   input, one expression per line (default: stdin)\n";
    std::exit(code);
}

Options parse_args(int argc, char **argv)
{
    Options o;
    bool    have_path = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if      (a == "-p") o.parse_only = true;
        else if (a == "-t") o.trace      = true;
        else if (a == "-a") o.strategy   = Strategy::Applicative;
        else if (a == "-e") o.eta        = true;
        else if (a == "-n") o.strategy   = Strategy::Normal;
        else if (a == "-h") usage(argv[0], 0);
        else if (a == "-s") {
            if (++i == argc)
                usage(argv[0], 2);
            o.step_limit = std::strtol(argv[i], nullptr, 10);
            if (o.step_limit <= 0)
                usage(argv[0], 2);
        }
        else if (a == "-")  { /* stdin */ }
        else if (a[0] == '-' && a.size() > 1) usage(argv[0], 2);
        else if (have_path) usage(argv[0], 2);
        else { o.path = a; have_path = true; }
    }
    return o;
}

} // namespace

int main(int argc, char **argv)
{
    const Options opt = parse_args(argc, argv);

    std::ifstream file;
    if (!opt.path.empty()) {
        file.open(opt.path);
        if (!file) {
            std::cerr << opt.path << ": cannot open\n";
            return 2;
        }
    }
    std::istream &in = opt.path.empty() ? std::cin : file;

    Lexer  lexer(in, std::cerr);
    Parser parser(lexer, std::cerr);
    Env    env;

    bool diverged = false;

    for (;;) {
        Line line = parser.next_line();
        if (line.kind == Line::Kind::End)
            break;
        if (line.kind == Line::Kind::Blank)
            continue;

        if (opt.parse_only) {
            /* Raw parse: no expansion, no reduction. */
            if (line.kind == Line::Kind::Definition)
                continue;
            print(*line.term, std::cout);
            std::cout << '\n';
            continue;
        }

        /* The right-hand side is expanded now, so a stored term is
         * self-contained and lookup can never recurse. */
        TermPtr expanded = env.expand(*line.term);

        if (line.kind == Line::Kind::Definition) {
            std::cout << line.name << " = ";
            print(*expanded, std::cout);
            std::cout << '\n';
            env.define(std::move(line.name), std::move(expanded));
            continue;
        }

        if (opt.trace) {
            std::cout << "    0  ";
            print(*expanded, std::cout);
            std::cout << '\n';
        }

        long   steps  = 0;
        Status status = Status::NormalForm;
        TermPtr result = reduce(std::move(expanded), opt.strategy, opt.eta,
                                opt.step_limit, opt.trace, steps, status,
                                std::cout);

        print(*result, std::cout);
        if (status == Status::Limit) {
            std::cout << "   [no normal form after " << steps << " steps]";
            diverged = true;
        }
        std::cout << '\n';
    }

    return (lexer.errors() || parser.errors() || diverged) ? 1 : 0;
}
