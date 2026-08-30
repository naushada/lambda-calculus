/* lexer.h — hand-written scanner over an istream.
 *
 * Replaces the flex specification this project started with.  The reason is
 * lambda: U+03BB is the two-byte sequence 0xCE 0xBB, and flex matches bytes
 * with no notion of a code point, so keeping a name from swallowing the
 * binder took a carefully constructed pair of negated byte classes.  With one
 * character of lookahead the same rule is a single comparison -- see
 * scan_name() and next() in lexer.cpp.
 *
 * Lookahead is a one-token pushback owned by the Lexer rather than
 * istream::putback, which is only guaranteed for a single character and can
 * fail depending on stream state.
 */
#ifndef LEXER_H
#define LEXER_H

#include <iosfwd>
#include <optional>
#include <string>

enum class Tok {
    End,        /* end of input          */
    Name,       /* identifier            */
    Lambda,     /* U+03BB, or backslash  */
    Dot,
    LParen,
    RParen,
    Eq,         /* binds a definition    */
    Newline     /* statement terminator  */
};

struct Token {
    Tok         kind = Tok::End;
    std::string text;       /* Name only */
    int         line = 1;
};

class Lexer {
public:
    Lexer(std::istream &in, std::ostream &err) : in_(in), err_(err) {}

    Token next();
    int   errors() const { return errors_; }

private:
    int  get();
    int  peek();
    void unget(int c);
    void error(const std::string &msg);

    Token scan_name(int first);

    std::istream        &in_;
    std::ostream        &err_;
    std::optional<Token> pending_;   /* produced when a name ends at a lambda */
    std::optional<int>   held_;      /* one-character pushback, ours not the stream's */
    int                  line_   = 1;
    int                  errors_ = 0;
};

#endif /* LEXER_H */
