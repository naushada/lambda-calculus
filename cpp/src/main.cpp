/* main.cpp — driver: read lines, bind definitions, reduce expressions. */
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
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

/* Handle one parsed line.  Shared by the batch loop and the REPL so the two
 * cannot drift: the REPL is a different way of *feeding* the parser, not a
 * different language. */
void handle(Line &line, const Options &opt, Env &env, bool &diverged)
{
    if (line.kind == Line::Kind::Blank || line.kind == Line::Kind::End)
        return;

    if (opt.parse_only) {
        if (line.kind == Line::Kind::Definition)
            return;                      /* raw parse: no expansion, no binding */
        print(*line.term, std::cout);
        std::cout << '\n';
        return;
    }

    /* The right-hand side is expanded now, so a stored term is
     * self-contained and lookup can never recurse. */
    TermPtr expanded = env.expand(*line.term);

    if (line.kind == Line::Kind::Definition) {
        std::cout << line.name << " = ";
        print(*expanded, std::cout);
        std::cout << '\n';
        env.define(std::move(line.name), std::move(expanded));
        return;
    }

    if (opt.trace) {
        std::cout << "    0  ";
        print(*expanded, std::cout);
        std::cout << '\n';
    }

    long    steps  = 0;
    Status  status = Status::NormalForm;
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

/* Interactive loop.
 *
 * Reads a whole line before scanning it, rather than letting the parser pull
 * tokens straight from std::cin.  The parser keeps two tokens of lookahead,
 * so pulling from the terminal would mean blocking for the *next* line before
 * the current one's result could be printed.  Feeding it a finished line
 * sidesteps that without changing the parser, and leaves the batch path --
 * and everything the test suite checks -- untouched.
 *
 * A fresh Lexer and Parser per line is deliberate: a syntax error cannot
 * strand state that affects the next entry.  `env` outlives the loop, so
 * definitions persist. */
int repl(const Options &opt, Env &env, bool &diverged)
{
    std::cout << "lambda calculus -- one expression per line, "
                 "Ctrl-D to exit\n";

    std::string text;
    int         errors   = 0;
    int         line_no  = 1;

    for (;;) {
        std::cout << "\xce\xbb> " << std::flush;      /* λ> */
        if (!std::getline(std::cin, text))
            break;                                     /* Ctrl-D */

        std::istringstream in(text + "\n");
        Lexer              lexer(in, std::cerr, line_no);
        Parser             parser(lexer, std::cerr);

        for (Line line = parser.next_line();
             line.kind != Line::Kind::End;
             line = parser.next_line())
            handle(line, opt, env, diverged);

        errors += lexer.errors() + parser.errors();
        line_no++;
    }

    std::cout << '\n';                                 /* past the prompt */
    return errors;
}

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

    Env  env;
    bool diverged = false;

    /* A prompt only when someone is there to read it: piping or redirecting
     * must produce byte-identical output to before. */
    if (opt.path.empty() && isatty(STDIN_FILENO))
        return (repl(opt, env, diverged) || diverged) ? 1 : 0;

    Lexer  lexer(in, std::cerr);
    Parser parser(lexer, std::cerr);

    for (Line line = parser.next_line();
         line.kind != Line::Kind::End;
         line = parser.next_line())
        handle(line, opt, env, diverged);

    return (lexer.errors() || parser.errors() || diverged) ? 1 : 0;
}
