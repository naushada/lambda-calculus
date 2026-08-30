/* parser.h — recursive descent.
 *
 *   expression   ::= name
 *                  | lambda <name> . <body>
 *                  | '(' <function expression> <argument expression> ')'
 *   definition   ::= <name> = <expression>          (top level only)
 *
 * Recursive descent rather than a generated parser because the grammar is
 * LL(1) by construction: application is explicitly bracketed, so every
 * alternative begins with a distinct token and every expression is
 * self-delimiting.  That is the same property that let the yacc version run
 * at zero conflicts -- here it means one function per production and a single
 * token of lookahead.  Distinguishing a definition from a bare expression is
 * the only place a second token is needed.
 */
#ifndef PARSER_H
#define PARSER_H

#include <iosfwd>
#include <string>

#include "ast.h"
#include "lexer.h"

struct Line {
    enum class Kind { Term, Definition, Blank, End };

    Kind        kind = Kind::End;
    std::string name;    /* Definition only */
    TermPtr     term;    /* Term and Definition */
};

class Parser {
public:
    Parser(Lexer &lex, std::ostream &err);

    Line next_line();
    int  errors() const { return errors_; }

private:
    void    advance();
    TermPtr expression();
    void    fail(const std::string &msg);   /* throws */
    void    resynchronise();                /* discard through the newline */

    Lexer        &lex_;
    std::ostream &err_;
    Token         cur_, ahead_;
    int           errors_ = 0;
};

#endif /* PARSER_H */
