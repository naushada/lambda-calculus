#include "lexer.h"

#include <cctype>
#include <istream>
#include <ostream>

namespace {

constexpr int LAMBDA_LEAD = 0xCE;   /* U+03BB is 0xCE 0xBB in UTF-8 */
constexpr int LAMBDA_TAIL = 0xBB;

bool is_blank(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

/* Characters that end a name.  Everything else -- including '#' and every
 * non-lambda UTF-8 byte -- may appear inside one, which is what makes the
 * language's "any sequence of non-blank characters" rule work for names like
 * x+y or pi2.  '#' opens a comment only when it starts a token. */
bool is_delim(int c)
{
    return c == EOF || is_blank(c) || c == '\n' || c == '.' || c == '(' ||
           c == ')' || c == '\\' || c == '=';
}

/* A UTF-8 continuation byte.  0xCE is always a lead byte (continuations are
 * 0x80-0xBF), so a 0xCE not followed by one is truncated input. */
bool is_continuation(int c)
{
    return c >= 0x80 && c <= 0xBF;
}

} // namespace

/* get()/peek() deal in int, never char: char is signed on most platforms, so
 * `char c = in.get(); c == 0xCE` is always false -- clang even warns that the
 * comparison can never be true. */
int Lexer::get()
{
    if (held_) {
        int c = *held_;
        held_.reset();
        return c;
    }
    return in_.get();
}

int Lexer::peek()
{
    return held_ ? *held_ : in_.peek();
}

/* Our own pushback rather than istream::putback, which is guaranteed for only
 * one character and can fail depending on stream state. */
void Lexer::unget(int c)
{
    if (c != EOF)
        held_ = c;
}

void Lexer::error(const std::string &msg)
{
    err_ << "line " << line_ << ": " << msg << '\n';
    errors_++;
}

/* Accumulate a name.  Stops at a delimiter, and at a lambda: the binder must
 * delimit even mid-word, so `x` U+03BB `y` is three tokens.  Detecting that
 * needs to look one byte past the 0xCE, so when it happens the lambda is
 * stashed in pending_ and handed out on the next call. */
Token Lexer::scan_name(int first)
{
    std::string text;
    const int   start_line = line_;
    int         c          = first;

    for (;;) {
        if (c == LAMBDA_LEAD) {
            int d = get();
            if (d == LAMBDA_TAIL) {
                pending_ = Token{Tok::Lambda, line_};
                break;
            }
            if (!is_continuation(d)) {
                error("illegal byte: truncated UTF-8 (0xce)");
                unget(d);
                break;
            }
            text += static_cast<char>(c);   /* a non-lambda 2-byte character */
            text += static_cast<char>(d);
        } else {
            text += static_cast<char>(c);
        }

        if (is_delim(peek()))
            break;
        c = get();
    }

    return Token{Tok::Name, start_line, std::move(text)};
}

Token Lexer::next()
{
    for (;;) {
        if (pending_) {
            Token t = std::move(*pending_);
            pending_.reset();
            return t;
        }

        int c = get();

        if (c == EOF)
            return Token{Tok::End, line_};

        if (is_blank(c))
            continue;

        if (c == '\n') {
            Token t{Tok::Newline, line_};
            line_++;                      /* the token reports the line it ends */
            return t;
        }

        if (c == '#') {                   /* comment to end of line */
            while (peek() != '\n' && peek() != EOF)
                get();
            continue;
        }

        switch (c) {
        case '.':  return Token{Tok::Dot,    line_};
        case '(':  return Token{Tok::LParen, line_};
        case ')':  return Token{Tok::RParen, line_};
        case '=':  return Token{Tok::Eq,     line_};
        case '\\': return Token{Tok::Lambda, line_};
        default:   break;
        }

        /* The entire byte-class problem of the flex version, in one line. */
        if (c == LAMBDA_LEAD && peek() == LAMBDA_TAIL) {
            get();
            return Token{Tok::Lambda, line_};
        }

        if (c == LAMBDA_LEAD && !is_continuation(peek())) {
            error("illegal byte: truncated UTF-8 (0xce)");
            continue;
        }

        if (std::isdigit(c)) {
            Token bad = scan_name(c);     /* consume it whole, then complain */
            error("name may not start with a digit: " + bad.text);
            continue;
        }

        return scan_name(c);
    }
}
