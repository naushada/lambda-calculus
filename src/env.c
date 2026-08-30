#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "eval.h"

typedef struct Def {
    char       *name;
    Node       *term;
    struct Def *next;
} Def;

static Def *defs = NULL;

static Def *find(const char *name)
{
    for (Def *d = defs; d; d = d->next)
        if (strcmp(d->name, name) == 0)
            return d;
    return NULL;
}

void env_define(char *name, Node *term)
{
    Def *d = find(name);

    if (d) {                       /* rebind */
        free_node(d->term);
        d->term = term;
        free(name);
        return;
    }
    d = malloc(sizeof *d);
    if (!d) {
        abort();
    }
    d->name = name;
    d->term = term;
    d->next = defs;
    defs = d;
}

/* Lambda binders in scope at the current point of the walk. */
typedef struct Bound {
    const char         *name;
    const struct Bound *next;
} Bound;

static int is_bound(const Bound *b, const char *name)
{
    for (; b; b = (const Bound *)b->next)
        if (strcmp(b->name, name) == 0)
            return 1;
    return 0;
}

static Node *expand(const Node *n, const Bound *bound)
{
    switch (n->kind) {
    case N_VAR: {
        Def *d;
        /* A binder shadows a definition of the same name. */
        if (is_bound(bound, n->name))
            return copy_node(n);
        if ((d = find(n->name)))
            return copy_node(d->term);
        return copy_node(n);
    }
    case N_ABS: {
        Bound b = { n->name, bound };
        return mk_abs(strdup(n->name), expand(n->l, &b));
    }
    case N_APP:
        return mk_app(expand(n->l, bound), expand(n->r, bound));
    }
    return NULL;
}

Node *env_expand(const Node *n) { return expand(n, NULL); }

void env_free(void)
{
    while (defs) {
        Def *next = defs->next;
        free_node(defs->term);
        free(defs->name);
        free(defs);
        defs = next;
    }
}
