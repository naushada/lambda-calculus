/* main.c — driver: parse each line, then reduce it unless told not to. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "env.h"
#include "eval.h"

extern FILE *yyin;
int  yyparse(void);
int  had_errors(void);

static Strategy strategy   = NORMAL_ORDER;
static long     step_limit = 10000;
static int      parse_only = 0;
static int      trace      = 0;
static int      diverged   = 0;   /* any term hit the step limit */

/* Called by the parser for a top-level `name = expr` line.  The right-hand
 * side is expanded now, so the stored term is self-contained and lookup can
 * never recurse. */
void on_define(char *name, Node *term)
{
    Node *expanded = env_expand(term);
    free_node(term);

    if (!parse_only) {
        printf("%s = ", name);
        print_node(expanded);
        putchar('\n');
    }
    env_define(name, expanded);
}

/* Called by the parser once per successfully parsed expression line. */
void on_term(Node *term)
{
    long       steps;
    EvalStatus status;

    if (parse_only) {
        print_node(term);
        putchar('\n');
        free_node(term);
        return;
    }

    {
        Node *expanded = env_expand(term);
        free_node(term);
        term = expanded;
    }

    if (trace) {
        fputs("    0  ", stdout);
        print_node(term);
        putchar('\n');
    }

    term = reduce(term, strategy, step_limit, trace, &steps, &status);

    print_node(term);
    if (status == EVAL_LIMIT) {
        printf("   [no normal form after %ld steps]", steps);
        diverged = 1;
    }
    putchar('\n');
    free_node(term);
}

static void usage(const char *prog, int code)
{
    fprintf(code ? stderr : stdout,
        "usage: %s [-p] [-t] [-a] [-s N] [file]\n"
        "  reads `expr` lines and `name = expr` definitions\n"
        "  -p     parse only; print the AST without reducing\n"
        "  -t     trace every reduction step\n"
        "  -a     applicative order (default: normal order)\n"
        "  -s N   step limit before giving up (default %ld)\n"
        "  file   input, one expression per line (default: stdin)\n",
        prog, step_limit);
    exit(code);
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0)      parse_only = 1;
        else if (strcmp(argv[i], "-t") == 0) trace = 1;
        else if (strcmp(argv[i], "-a") == 0) strategy = APPLICATIVE_ORDER;
        else if (strcmp(argv[i], "-n") == 0) strategy = NORMAL_ORDER;
        else if (strcmp(argv[i], "-h") == 0) usage(argv[0], 0);
        else if (strcmp(argv[i], "-s") == 0) {
            if (++i == argc)
                usage(argv[0], 2);
            step_limit = strtol(argv[i], NULL, 10);
            if (step_limit <= 0)
                usage(argv[0], 2);
        }
        else if (strcmp(argv[i], "-") == 0)  path = NULL;
        else if (argv[i][0] == '-')          usage(argv[0], 2);
        else if (path)                       usage(argv[0], 2);
        else                                 path = argv[i];
    }

    if (path) {
        yyin = fopen(path, "r");
        if (!yyin) {
            perror(path);
            return 2;
        }
    }

    yyparse();
    env_free();

    if (path)
        fclose(yyin);
    return (had_errors() || diverged) ? 1 : 0;
}
