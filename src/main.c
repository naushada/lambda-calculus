/* main.c — driver: parse each line of a file or stdin, print the AST. */
#include <stdio.h>
#include <string.h>

extern FILE *yyin;
int  yyparse(void);
int  had_errors(void);

int main(int argc, char **argv)
{
    const char *path = NULL;

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
        fprintf(stderr, "usage: %s [file]   (reads stdin if no file)\n",
                argv[0]);
        return 2;
    }
    if (argc == 2 && strcmp(argv[1], "-") != 0)
        path = argv[1];

    if (path) {
        yyin = fopen(path, "r");
        if (!yyin) {
            perror(path);
            return 2;
        }
    }

    yyparse();

    if (path)
        fclose(yyin);
    return had_errors() ? 1 : 0;
}
