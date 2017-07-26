#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parse.h"
#include "util.h"

// Exit the main interactive loop
int shell_do_exit = 0;

// Various flags.
int obscene_debug = 0;

int main(int argc, char **argv) {
    char* run_str = NULL;
    // Options.
    int c;
    while ((c = getopt (argc, argv, "Dc:")) != -1) {
        switch(c) {
            case 'D':
                obscene_debug = 1;
                break;
            case 'c':
                run_str = optarg;
                break;
            case '?':
                printf("Invalid invocation.\n");
                return 1;
                break;
            default:
                assert(0); // Seriously, how?
                break;
        }
    }

    if (run_str) {
        ast_t *toks = parse(run_str);
        toks        = resolve(toks);
        execute(toks, NULL);
        free(toks->ptr);
        free(toks);
    } else {
        while (!shell_do_exit) {
            // Read a command in.
            char *input  = read_input();
            ast_t *toks  = parse(input);
            toks         = resolve(toks);
            execute(toks, NULL);
            free(toks->ptr);
            free(toks);
            // FIXME: free the damn toks memory, this requires traversing to the bottom tho
            free(input);
        }
    }
}
