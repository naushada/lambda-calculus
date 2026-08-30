/* dump.cpp — print the token stream the scanner produces.
 *
 * A debugging view of cpp/src/lexer.cpp in isolation, with no parser in the
 * way.  Useful when a term does not parse and the question is whether it was
 * tokenised as intended -- UTF-8 input especially, where the binder is two
 * bytes and a name must stop before it.
 *
 * The verified token vectors in docs/CPP_SCANNER_DESIGN.md were generated
 * with this.
 *
 *   make tokens                       builds cpp/build/lc-tokens
 *   echo 'λx.x' | cpp/build/lc-tokens
 *   cpp/build/lc-tokens -v file.lc
 */
#include <fstream>
#include <iostream>
#include <string>

#include "lexer.h"

namespace {

const char *name_of(Tok t)
{
    /* A switch rather than a table: -Wswitch then makes a new Tok member a
     * compile-time error here instead of a silently wrong label. */
    switch (t) {
    case Tok::End:     return "EOF";
    case Tok::Name:    return "NAME";
    case Tok::Lambda:  return "LAMBDA";
    case Tok::Dot:     return "DOT";
    case Tok::LParen:  return "LPAREN";
    case Tok::RParen:  return "RPAREN";
    case Tok::Eq:      return "EQ";
    case Tok::Newline: return "NEWLINE";
    }
    return "?";
}

void usage(const char *prog, int code)
{
    (code ? std::cerr : std::cout)
        << "usage: " << prog << " [-v] [file]\n"
        << "  prints the token stream, one input line per output line\n"
        << "  -v     verbose: one token per line, with line numbers\n"
        << "  file   input (default: stdin)\n";
    std::exit(code);
}

} // namespace

int main(int argc, char **argv)
{
    bool        verbose = false;
    std::string path;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if      (a == "-v") verbose = true;
        else if (a == "-h") usage(argv[0], 0);
        else if (a == "-")  { /* stdin */ }
        else if (a.size() > 1 && a[0] == '-') usage(argv[0], 2);
        else if (!path.empty()) usage(argv[0], 2);
        else path = a;
    }

    std::ifstream file;
    if (!path.empty()) {
        file.open(path);
        if (!file) {
            std::cerr << path << ": cannot open\n";
            return 2;
        }
    }
    std::istream &in = path.empty() ? std::cin : file;

    Lexer lexer(in, std::cerr);
    bool  empty_line = true;    /* nothing printed for the current line yet */

    for (;;) {
        Token t = lexer.next();

        if (verbose) {
            std::cout << "line " << t.line << "\t" << name_of(t.kind);
            if (t.kind == Tok::Name)
                std::cout << "\t" << t.text;
            std::cout << '\n';
            if (t.kind == Tok::End)
                break;
            continue;
        }

        switch (t.kind) {
        case Tok::End:
            /* A trailing token run with no closing newline still gets one. */
            if (!empty_line)
                std::cout << '\n';
            std::cout << "EOF\n";
            return lexer.errors() ? 1 : 0;

        case Tok::Newline:
            std::cout << '\n';
            empty_line = true;
            break;

        default:
            if (!empty_line)
                std::cout << ' ';
            std::cout << name_of(t.kind);
            if (t.kind == Tok::Name)
                std::cout << '(' << t.text << ')';
            empty_line = false;
            break;
        }
    }

    return lexer.errors() ? 1 : 0;
}
