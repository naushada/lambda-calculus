/* main.c — driver: parse each line, then reduce it unless told not to. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "env.h"
#include "eval.h"

extern FILE *yyin;
int  yyparse(void);
int  had_errors(void);

/* Scanning a string rather than a stream, for the REPL.  Declared by hand so
 * main.c does not have to include the generated scanner's header. */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_scan_string(const char *str);
void            yy_delete_buffer(YY_BUFFER_STATE buf);
extern int      yylineno;
extern int      tok_line;

static Strategy strategy   = NORMAL_ORDER;
static long     step_limit = 10000;
static int      parse_only = 0;
static int      eta        = 0;
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

    term = reduce(term, strategy, eta, step_limit, trace, &steps, &status);

    print_node(term);
    if (status == EVAL_LIMIT) {
        printf("   [no normal form after %ld steps]", steps);
        diverged = 1;
    }
    putchar('\n');
    free_node(term);
}

/* Interactive loop.
 *
 * Reads a whole line, then scans that string, rather than letting yyparse
 * pull tokens straight from stdin.  The parser holds a lookahead token, so
 * pulling from the terminal would mean blocking for the *next* line before
 * the current one's result could be printed.  Feeding it a finished line
 * sidesteps that without touching the grammar, and leaves the batch path --
 * and everything the test suite checks -- untouched.
 *
 * The definition table is global and outlives the loop, so definitions
 * persist between prompts. */
static void repl(void)
{
    char  *line = NULL;
    size_t cap  = 0;
    int    lineno = 1;

    puts("lambda calculus -- one expression per line, Ctrl-D to exit");

    for (;;) {
        ssize_t n;
        char   *buf;

        fputs("\xce\xbb> ", stdout);          /* λ> */
        fflush(stdout);

        n = getline(&line, &cap, stdin);
        if (n < 0)
            break;                              /* Ctrl-D */

        /* The grammar terminates a line with a newline; getline drops it at
         * end of file without one. */
        buf = malloc((size_t)n + 2);
        if (!buf)
            break;
        memcpy(buf, line, (size_t)n);
        if (n == 0 || buf[n - 1] != '\n')
            buf[n++] = '\n';
        buf[n] = '\0';

        yylineno = tok_line = lineno;
        {
            YY_BUFFER_STATE b = yy_scan_string(buf);
            yyparse();
            yy_delete_buffer(b);
        }
        free(buf);
        lineno++;
    }

    free(line);
    putchar('\n');                             /* past the prompt */
}

static void usage(const char *prog, int code)
{
    fprintf(code ? stderr : stdout,
        "usage: %s [-p] [-t] [-a] [-e] [-s N] [file]\n"
        "  reads `expr` lines and `name = expr` definitions\n"
        "  with no file and a terminal on stdin, starts an interactive REPL\n"
        "  -p     parse only; print the AST without reducing\n"
        "  -t     trace every reduction step\n"
        "  -a     applicative order (default: normal order)\n"
        "  -e     also apply eta reduction: lambda x.(M x) -> M\n"
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
        else if (strcmp(argv[i], "-e") == 0) eta = 1;
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

    /* A prompt only when someone is there to read it: piping or redirecting
     * must produce byte-identical output to before. */
    if (!path && isatty(STDIN_FILENO)) {
        repl();
        env_free();
        return (had_errors() || diverged) ? 1 : 0;
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
