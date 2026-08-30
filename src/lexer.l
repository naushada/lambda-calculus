/* lexer.l — scanner for the lambda calculus.  See docs/LEX_DESIGN.md
 *
 * The binder is the Greek letter lambda, U+03BB, written here as the UTF-8
 * escape "\xce\xbb" so this file stays pure ASCII.  A backslash is accepted
 * as a fallback for keyboards that cannot produce it.
 *
 * CEOK is the crux (LEX_DESIGN 2.3): flex matches bytes, not code points, so
 * a name must be prevented from swallowing the two bytes of the binder while
 * still admitting every other 2-byte 0xCE character (mu, pi, ...).  The
 * trailing range [\x80-\xba\xbc-\xbf] is exactly "a UTF-8 continuation byte
 * that is not 0xBB", which also makes a truncated 0xCE fail and be reported
 * rather than absorbed into a name.
 *
 * `=` is excluded from names so that top-level definitions (`id = expr`) can
 * be written without spaces.  This was the change LEX_DESIGN 8 predicted; it
 * is the only reason `=` is not an ordinary name character.
 */
%{
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.tab.h"

int lex_errors = 0;   /* scanner-level diagnostics; see main.c   */
int tok_line   = 1;   /* line of the token just returned         */

static void lex_error(const char *fmt, ...)
{
    va_list ap;
    fflush(stdout);          /* keep diagnostics in step with results when
                              * stdout is a pipe and thus block-buffered */
    fprintf(stderr, "line %d: ", yylineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    lex_errors++;
}

/* Record the line a token starts on.  yylineno has already moved past a
 * newline by the time its action runs, so NEWLINE reports the line it ends. */
#define TOK(t)  do { tok_line = yylineno; return (t); } while (0)
%}

%option noyywrap yylineno 8bit
%option nounput noinput

DIGIT     [0-9]
CEOK      \xce[\x80-\xba\xbc-\xbf]
NOTCE     [^ \t\r\f\v\n.()\\=\xce]
NOTCE0    [^0-9 \t\r\f\v\n.()\\=\xce]

IDSTART   {NOTCE0}|{CEOK}
IDCHAR    {NOTCE}|{CEOK}
NAME      {IDSTART}{IDCHAR}*
BADNAME   {DIGIT}{IDCHAR}*

%%

  /* --- discarded --- */
[ \t\r\f\v]+    { /* blanks separate tokens, nothing more */ }
"#".*           { /* comment to end of line; the newline is left for below */ }

  /* --- statement terminator (YACC_DESIGN 4) --- */
\n              { tok_line = yylineno - 1; return NEWLINE; }

  /* --- binder: U+03BB, plus an ASCII fallback --- */
"\xce\xbb"      { TOK(LAMBDA); }
"\\"            { TOK(LAMBDA); }

  /* --- punctuation --- */
"="             { TOK(EQ); }
"."             { TOK(DOT); }
"("             { TOK(LPAREN); }
")"             { TOK(RPAREN); }

  /* --- identifiers --- */
{NAME}          { yylval.sval = strdup(yytext); TOK(NAME); }

  /* --- diagnostics --- */
{BADNAME}       { lex_error("name may not start with a digit: %s", yytext); }
.               { lex_error("illegal byte: 0x%02x",
                            (unsigned char)yytext[0]); }

%%
