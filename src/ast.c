#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

static Node *alloc(NodeKind kind)
{
    Node *n = calloc(1, sizeof *n);
    if (!n) {
        fputs("out of memory\n", stderr);
        exit(2);
    }
    n->kind = kind;
    return n;
}

Node *mk_var(char *name)
{
    Node *n = alloc(N_VAR);
    n->name = name;
    return n;
}

Node *mk_abs(char *name, Node *body)
{
    Node *n = alloc(N_ABS);
    n->name = name;
    n->l = body;
    return n;
}

Node *mk_app(Node *fn, Node *arg)
{
    Node *n = alloc(N_APP);
    n->l = fn;
    n->r = arg;
    return n;
}

void print_node(const Node *n)
{
    if (!n)
        return;
    switch (n->kind) {
    case N_VAR:
        fputs(n->name, stdout);
        break;
    case N_ABS:
        printf("(\xce\xbb%s.", n->name);   /* U+03BB */
        print_node(n->l);
        putchar(')');
        break;
    case N_APP:
        putchar('(');
        print_node(n->l);
        putchar(' ');
        print_node(n->r);
        putchar(')');
        break;
    }
}

void free_node(Node *n)
{
    if (!n)
        return;
    free_node(n->l);
    free_node(n->r);
    free(n->name);
    free(n);
}
