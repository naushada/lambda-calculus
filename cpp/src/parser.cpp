#include "parser.h"

#include <ostream>
#include <stdexcept>

namespace {
struct SyntaxError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
} // namespace

Parser::Parser(Lexer &lex, std::ostream &err) : lex_(lex), err_(err)
{
    cur_   = lex_.next();
    ahead_ = lex_.next();
}

void Parser::advance()
{
    cur_   = std::move(ahead_);
    ahead_ = lex_.next();
}

void Parser::fail(const std::string &msg)
{
    err_ << "line " << cur_.line << ": " << msg << '\n';
    errors_++;
    throw SyntaxError(msg);
}

void Parser::resynchronise()
{
    while (cur_.kind != Tok::Newline && cur_.kind != Tok::End)
        advance();
}

TermPtr Parser::expression()
{
    switch (cur_.kind) {
    case Tok::Name: {
        std::string name = std::move(cur_.text);
        advance();
        return mk_var(std::move(name));
    }

    case Tok::Lambda: {
        advance();
        if (cur_.kind != Tok::Name)
            fail("syntax error: expected a name after the binder");
        std::string param = std::move(cur_.text);
        advance();
        if (cur_.kind != Tok::Dot)
            fail("syntax error: expected '.' after the bound name");
        advance();
        return mk_abs(std::move(param), expression());
    }

    case Tok::LParen: {
        advance();
        TermPtr fn = expression();

        /* Brackets are application syntax, so exactly two expressions are
         * required.  Saying so beats a bare "syntax error": the parser knows
         * precisely what is wrong. */
        if (cur_.kind == Tok::RParen)
            fail("an application needs two expressions: (function argument)");

        TermPtr arg = expression();
        if (cur_.kind != Tok::RParen)
            fail("syntax error: expected ')' to close the application");
        advance();
        return mk_app(std::move(fn), std::move(arg));
    }

    default:
        fail("syntax error: expected an expression");
    }
    return nullptr;   /* unreachable: fail() throws */
}

Line Parser::next_line()
{
    for (;;) {
        if (cur_.kind == Tok::End)
            return Line{Line::Kind::End, "", nullptr};

        if (cur_.kind == Tok::Newline) {
            advance();
            return Line{Line::Kind::Blank, "", nullptr};
        }

        try {
            /* The one place two tokens are needed: a name followed by '='
             * begins a definition, otherwise it is an expression. */
            if (cur_.kind == Tok::Name && ahead_.kind == Tok::Eq) {
                std::string name = std::move(cur_.text);
                advance();                       /* past the name */
                advance();                       /* past the '='  */
                TermPtr body = expression();
                if (cur_.kind != Tok::Newline && cur_.kind != Tok::End)
                    fail("syntax error: trailing input after the definition");
                if (cur_.kind == Tok::Newline)
                    advance();
                return Line{Line::Kind::Definition, std::move(name),
                            std::move(body)};
            }

            TermPtr term = expression();
            if (cur_.kind != Tok::Newline && cur_.kind != Tok::End)
                fail("syntax error: trailing input after the expression");
            if (cur_.kind == Tok::Newline)
                advance();
            return Line{Line::Kind::Term, "", std::move(term)};
        }
        catch (const SyntaxError &) {
            /* Resynchronise at the newline so one bad line neither aborts the
             * run nor cascades into the next. */
            resynchronise();
            if (cur_.kind == Tok::Newline)
                advance();
            return Line{Line::Kind::Blank, "", nullptr};
        }
    }
}
