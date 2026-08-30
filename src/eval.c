#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"

/* ---------------------------------------------------------------- sets --
 * A name set, used only for free-variable queries during substitution.
 * Terms here are small; linear scan is the right trade.
 */
typedef struct {
    char **v;
    size_t n, cap;
} StrSet;

static void set_init(StrSet *s) { s->v = NULL; s->n = s->cap = 0; }

static int set_has(const StrSet *s, const char *name)
{
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], name) == 0)
            return 1;
    return 0;
}

static void set_add(StrSet *s, const char *name)
{
    if (set_has(s, name))
        return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = realloc(s->v, s->cap * sizeof *s->v);
        if (!s->v) {
            fputs("out of memory\n", stderr);
            exit(2);
        }
    }
    s->v[s->n++] = (char *)name;   /* borrowed from the tree, never freed */
}

static void set_free(StrSet *s) { free(s->v); set_init(s); }

/* Free variables of `n`, accumulated into `out`. */
static void free_vars(const Node *n, StrSet *out)
{
    switch (n->kind) {
    case N_VAR:
        set_add(out, n->name);
        break;
    case N_APP:
        free_vars(n->l, out);
        free_vars(n->r, out);
        break;
    case N_ABS: {
        StrSet inner;
        set_init(&inner);
        free_vars(n->l, &inner);
        for (size_t i = 0; i < inner.n; i++)
            if (strcmp(inner.v[i], n->name) != 0)
                set_add(out, inner.v[i]);   /* the binder is not free */
        set_free(&inner);
        break;
    }
    }
}

/* ---------------------------------------------------------------- trees -- */

Node *copy_node(const Node *n)
{
    if (!n)
        return NULL;
    switch (n->kind) {
    case N_VAR: return mk_var(strdup(n->name));
    case N_ABS: return mk_abs(strdup(n->name), copy_node(n->l));
    case N_APP: return mk_app(copy_node(n->l), copy_node(n->r));
    }
    return NULL;
}

/* A name like `x` that appears in neither set.  Tries x1, x2, ... which stay
 * legal identifiers under the scanner (a name may end with digits). */
static char *fresh_name(const char *base, const StrSet *a, const StrSet *b)
{
    size_t len = strlen(base) + 24;
    char *cand = malloc(len);
    if (!cand) {
        fputs("out of memory\n", stderr);
        exit(2);
    }
    for (unsigned i = 1; ; i++) {
        snprintf(cand, len, "%s%u", base, i);
        if (!set_has(a, cand) && !set_has(b, cand))
            return cand;
    }
}

/* body[name := value], capture-avoiding. */
Node *substitute(const Node *body, const char *name, const Node *value)
{
    switch (body->kind) {
    case N_VAR:
        return strcmp(body->name, name) == 0 ? copy_node(value)
                                             : copy_node(body);
    case N_APP:
        return mk_app(substitute(body->l, name, value),
                      substitute(body->r, name, value));
    case N_ABS: {
        /* The binder shadows `name`: nothing inside is free for it. */
        if (strcmp(body->name, name) == 0)
            return copy_node(body);

        StrSet fv_val;
        set_init(&fv_val);
        free_vars(value, &fv_val);

        /* The binder does not occur free in `value`, so no capture is
         * possible and we can descend directly. */
        if (!set_has(&fv_val, body->name)) {
            set_free(&fv_val);
            return mk_abs(strdup(body->name),
                          substitute(body->l, name, value));
        }

        /* Capture would occur: alpha-rename the binder first.  The new name
         * must avoid the free variables of `value` (what we are inserting)
         * and of the body (what is already there). */
        StrSet fv_body;
        set_init(&fv_body);
        free_vars(body->l, &fv_body);

        char *z = fresh_name(body->name, &fv_val, &fv_body);
        Node *zvar    = mk_var(strdup(z));
        Node *renamed = substitute(body->l, body->name, zvar);
        Node *result  = mk_abs(z, substitute(renamed, name, value));

        free_node(zvar);
        free_node(renamed);
        set_free(&fv_val);
        set_free(&fv_body);
        return result;                      /* `z` is owned by the node */
    }
    }
    return NULL;
}

/* Contract a redex: (\x.M N) -> M[x := N] */
static Node *beta(const Node *abs, const Node *arg)
{
    return substitute(abs->l, abs->name, arg);
}

/* Eta: \x.(M x) -> M, provided x is not free in M.
 *
 * The side condition is the whole rule.  Without it \x.(x x) would collapse
 * to x, which is a different function: the abstraction uses its argument
 * twice, so discarding the binder changes meaning.  Returns NULL when the
 * term is not an eta-redex. */
static Node *try_eta(const Node *abs)
{
    const Node *body = abs->l;
    StrSet      fv;
    int         captured;

    if (body->kind != N_APP || body->r->kind != N_VAR)
        return NULL;
    if (strcmp(body->r->name, abs->name) != 0)
        return NULL;

    set_init(&fv);
    free_vars(body->l, &fv);
    captured = set_has(&fv, abs->name);
    set_free(&fv);

    if (captured)
        return NULL;             /* x occurs free in M: not an eta-redex */

    return copy_node(body->l);
}

Node *reduce_step(const Node *n, Strategy s, int eta)
{
    Node *step;

    if (s == NORMAL_ORDER) {
        /* Outermost first: contract this redex before touching the argument. */
        if (n->kind == N_APP && n->l->kind == N_ABS)
            return beta(n->l, n->r);
    }

    switch (n->kind) {
    case N_VAR:
        return NULL;

    case N_ABS:
        /* Outermost first, mirroring how beta is ordered above. */
        if (eta && s == NORMAL_ORDER && (step = try_eta(n)))
            return step;

        if ((step = reduce_step(n->l, s, eta)))
            return mk_abs(strdup(n->name), step);

        /* Innermost: the body is in normal form, so contract now. */
        if (eta && s == APPLICATIVE_ORDER && (step = try_eta(n)))
            return step;

        return NULL;

    case N_APP:
        /* Leftmost: the function position before the argument position. */
        if ((step = reduce_step(n->l, s, eta)))
            return mk_app(step, copy_node(n->r));
        if ((step = reduce_step(n->r, s, eta)))
            return mk_app(copy_node(n->l), step);
        /* Innermost: both parts are in normal form, so contract now. */
        if (s == APPLICATIVE_ORDER && n->l->kind == N_ABS)
            return beta(n->l, n->r);
        return NULL;
    }
    return NULL;
}

Node *reduce(Node *term, Strategy s, int eta, long limit, int trace,
             long *steps, EvalStatus *status)
{
    long n = 0;

    for (; n < limit; n++) {
        Node *next = reduce_step(term, s, eta);
        if (!next) {
            *steps  = n;
            *status = EVAL_NORMAL_FORM;
            return term;
        }
        free_node(term);
        term = next;
        if (trace) {
            printf("  %3ld  ", n + 1);
            print_node(term);
            putchar('\n');
        }
    }
    *steps  = n;
    *status = EVAL_LIMIT;
    return term;
}
