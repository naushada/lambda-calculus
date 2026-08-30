/* parser.y — grammar for the lambda calculus.  See docs/YACC_DESIGN.md
 *
 *   expression  ::= name
 *                 | function
 *                 | application
 *   function    ::= lambda <name> . <body>
 *   body        ::= expression
 *   application ::= '(' <function expression> <argument expression> ')'
 *
 * Application is explicitly parenthesised and binary, which is what makes the
 * grammar unambiguous with no precedence declarations at all: every
 * alternative starts with a distinct token and every expression is
 * self-delimiting.  Build must stay at 0 conflicts -- `make conflicts`.
 */
%{
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"

int   yylex(void);
void  yyerror(const char *msg);

/* Supplied by the driver: what to do with a completed term.  Takes
 * ownership of the node. */
void  on_term(struct Node *term);

extern int yylineno;
extern int tok_line;
extern int lex_errors;

int parse_errors = 0;
%}

%union {
    char        *sval;
    struct Node *node;
}

%token <sval> NAME
%token LAMBDA DOT LPAREN RPAREN NEWLINE

%type <node> expr

/* Reclaim semantic values popped during error recovery.  Per-symbol form:
 * the typed form `%destructor { ... } <sval>` needs bison >= 2.4, and Apple's
 * system bison is 2.3. */
%destructor { free($$); }      NAME
%destructor { free_node($$); } expr

%start program

%%

program : /* empty */
        | program line
        ;

line    : expr NEWLINE          { on_term($1); }
        | NEWLINE               { /* blank line */ }
        | error NEWLINE         { parse_errors++; yyerrok; }
        ;

expr    : NAME                  { $$ = mk_var($1); }
        | LAMBDA NAME DOT expr  { $$ = mk_abs($2, $4); }
        | LPAREN expr expr RPAREN
                                { $$ = mk_app($2, $3); }

          /* Not grammar, but a better diagnostic: brackets are application
           * syntax, so exactly two expressions are required.  Writing
           * `(f x)` is right; `(f)` is the common beginner mistake.
           * To allow brackets as plain grouping instead (YACC_DESIGN 7),
           * replace this action with  { $$ = $2; }  -- still 0 conflicts. */
        | LPAREN expr RPAREN    { free_node($2);
                                  $$ = NULL;   /* the expr destructor may
                                                * run on this; keep it sane */
                                  yyerror("an application needs two "
                                          "expressions: (function argument)");
                                  YYERROR; }
        ;

%%

void yyerror(const char *msg)
{
    fprintf(stderr, "line %d: %s\n", tok_line, msg);
}

int had_errors(void)
{
    return parse_errors || lex_errors;
}
