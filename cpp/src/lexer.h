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

/* Field order is layout, not taste.  A member starts at the next offset that
 * is a multiple of its own alignment, and the total rounds up to a multiple
 * of the struct's alignment, so splitting the two ints with the 24-byte
 * string would cost four bytes in front of it and four at the tail:
 *
 *   kind, text, line -> 4 + [4 pad] + 24 + 4 + [4 pad] = 40
 *   kind, line, text -> 4 +     4   + 24               = 32
 *
 * Keeping members of equal alignment adjacent is the whole trick; the
 * compiler may not do it for you, because C++ lays members out in
 * declaration order.  docs/CPP_SCANNER_DESIGN.md 5 states the rules.
 *
 * text is last also because it is the only optional field -- a token that
 * carries no text is written Token{Tok::Dot, line_}. */
struct Token {
    Tok         kind = Tok::End;
    int         line = 1;
    std::string text;       /* Name only */

    Token() = default;
    /* The two-argument form is the common one: only Name carries text.  It
     * exists because leaving text off an aggregate initializer is what
     * -Wextra calls a missing field initializer. */
    Token(Tok k, int ln) : kind(k), line(ln) {}
    Token(Tok k, int ln, std::string t) : kind(k), line(ln), text(std::move(t)) {}
};

class Lexer {
public:
    /* `first_line` lets a caller that feeds the scanner one line at a time --
     * the REPL -- keep diagnostics numbered against the whole session. */
    Lexer(std::istream &in, std::ostream &err, int first_line = 1)
        : in_(in), err_(err), line_(first_line) {}

    Token next();
    int   errors() const { return errors_; }

private:
    int  get();
    int  peek();
    void unget(int c);
    void error(const std::string &msg);

    Token scan_name(int first);

    /* Memory layout (LP64: x86-64 / arm64, libc++ or libstdc++).  A Lexer is
     * 72 bytes, 8-byte aligned, and owns no heap of its own -- the only
     * allocation it can make is the std::string inside pending_, and only for
     * a Name, which pending_ never holds.
     *
     *   off  size  member
     *     0     8  in_        istream*  (a reference is a pointer here)
     *     8     8  err_       ostream*
     *    16    40  pending_   optional<Token>
     *    56     8  held_      optional<int>
     *    64     4  line_
     *    68     4  errors_
     *
     * These six are packed: held_ at 56 is already 4-aligned and the raw 72
     * is already a multiple of 8, so no reordering removes a byte.  The 10
     * bytes of slack are one level down, inside the optionals, where the
     * round-up-to-alignment rule applies to fields this header does not own:
     *
     *   optional<Token>  Token at 0, engaged flag at 32, 7 bytes tail = 40
     *   optional<int>    int   at 0, engaged flag at  4, 3 bytes tail =  8
     *
     * Recovering those means sentinels instead of optionals -- int held_ =
     * EOF, kind == Tok::End for "nothing pending" -- trading a type-enforced
     * empty for space this class does not need: the driver and the REPL each
     * hold exactly one Lexer.  Token is the type worth packing, since one is
     * copied and moved per token scanned.  docs/CPP_SCANNER_DESIGN.md 5.
     *
     * (Offsets measured, not derived: Apple clang/libc++ on arm64.  libstdc++
     * lays an optional out the same way -- payload first, flag after.)
     *
     * Every member is a plain value: no vtable, no owned buffer, no
     * destructor beyond the string's.  The scanner's whole mutable state is
     * these two optionals plus two ints, which is why the class is cheap
     * enough for the REPL to construct a fresh one per input line.  The
     * streams are referenced, not owned; a Lexer must not outlive them. */
    std::istream        &in_;
    std::ostream        &err_;
    std::optional<Token> pending_;   /* produced when a name ends at a lambda */
    std::optional<int>   held_;      /* one-character pushback, ours not the stream's */
    int                  line_;
    int                  errors_ = 0;
};

#endif /* LEXER_H */
