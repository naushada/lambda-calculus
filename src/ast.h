/* ast.h — abstract syntax tree for the lambda calculus.
 *
 * Node shapes (see docs/YACC_DESIGN.md §2.2):
 *   N_VAR   name = the variable
 *   N_ABS   name = the bound name, l = body
 *   N_APP   l = function expression, r = argument expression
 *
 * Ownership: `name` is a heap string handed over by the scanner (which
 * strdup()s every NAME) and freed by free_node().
 */
#ifndef AST_H
#define AST_H

typedef enum { N_VAR, N_ABS, N_APP } NodeKind;

typedef struct Node {
    NodeKind      kind;
    char         *name;
    struct Node  *l, *r;
} Node;

Node *mk_var(char *name);             /* takes ownership of name */
Node *mk_abs(char *name, Node *body); /* takes ownership of name */
Node *mk_app(Node *fn, Node *arg);

void  print_node(const Node *n);      /* fully parenthesised, to stdout */
void  free_node(Node *n);             /* recursive; NULL-safe */

#endif /* AST_H */
