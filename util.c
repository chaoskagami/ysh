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

builtin_info_t builtin_info[] = {
    { "cd",  builtin_chdir },

    { "+",   builtin_add },
    { "x+",  builtin_add },
    { "X+",  builtin_add },
    { "o+",  builtin_add },

    { "-",   builtin_sub },
    { "x-",  builtin_sub },
    { "X-",  builtin_sub },
    { "o-",  builtin_sub },

    { "*",   builtin_mul },
    { "x*",  builtin_mul },
    { "X*",  builtin_mul },
    { "o*",  builtin_mul },

    { "/",   builtin_div },
    { "x/",  builtin_div },
    { "X/",  builtin_div },
    { "o/",  builtin_div },

    { "%",   builtin_modulo },
    { "x%",  builtin_modulo },
    { "X%",  builtin_modulo },
    { "o%",  builtin_modulo },

    { "",   NULL },
};

void* malloc_trap(size_t malloc_size) {
    void* ret = malloc(malloc_size);
    if (!ret) {
        perror("err: malloc_trap: \n");
        exit(EXIT_FAILURE);
    }
    return ret;
}

void* realloc_trap(void *ptr, size_t malloc_size) {
    void* ret = realloc(ptr, malloc_size);
    if (!ret) {
        perror("err: realloc_trap: \n");
        exit(EXIT_FAILURE);
    }
    return ret;
}

/* Reads input from the user; this includes single lines, as well as
 * escaped multi-line input.
 *
 * This is essentially an implementation of getline.
 */
char *read_input() {
    // We do not know the size of the buffer ahead of time; therefore,
    // we must reallocate as we need more space.

    // The calling loop will free the allocated string when we are finished,
    // so we do not decrease our buffer size, only increase.

    // To avoid allocation overhead, we work in a "chunk" size specified by
    // BUF_CHUNKSIZ, and each time the buffer must be grown, we double
    // the currently allocated size to try and avoid problems.
    size_t buffer_sz = BUF_CHUNKSIZ;
    char *buffer = (char*)malloc_trap(buffer_sz);
    size_t pos = 0;
    int c = 0, last_c = 0;

    while (1) {
        c = getchar();

        if (c == '\b') {
            // Backspace
            if (pos && buffer[pos] != '\n') { // Can't delete newlines.
                --pos;
                if (pos)
                    last_c = buffer[pos-1];
            }
            continue;
        } else if (c == '\n' && last_c == '\\') {
            // Terminate reading input when EOF or if we receive an "unescaped" newline.
            --pos; // Don't copy the '\\' into output.
            fflush(stdout);
        } else if (c == EOF || c == '\n') {
            buffer[pos] = 0;
            return buffer;
        }

        buffer[pos] = c;
        ++pos;

        if (pos >= buffer_sz) {
            buffer_sz *= 2;
            buffer = realloc_trap(buffer, buffer_sz);
        }

        last_c = c;
    }
}

int check_builtin(char* name) {
    for (int i = 0; builtin_info[i].func != NULL; i++) {
        if (!strcmp(name, builtin_info[i].name)) {
            return i;
        }
    }
    return -1;
}

pid_t fork_and_execvp(const char *file, char *const argv[], char** stdout) {
    pid_t pid;

    int pipefd[2];
    if (stdout)
        pipe(pipefd);

    pid = fork();

    if (pid == 0) {
        if (stdout) {
            // Close the rx pipe in child.
            close(pipefd[0]);
            dup2(pipefd[1], 1); // stdout -> pipe
            close(pipefd[1]);
        }

        execvp(file, argv);
    } else {
        if (stdout) {
            size_t stdout_sz = 4096, stdout_at = 0;
            *stdout = NULL;
            char * cur = *stdout;

            // Close the tx pipe in parent.
            close(pipefd[1]);
            ssize_t bytes = 0;
            do {
                stdout_at++;
                *stdout = realloc_trap(*stdout, stdout_sz * stdout_at);
                cur = *stdout + (stdout_sz * (stdout_at-1));
                memset(cur, 0, 4096);

                bytes = read(pipefd[0], cur, stdout_sz);
            } while (bytes != 0);

            // Strip the last \n from output, if applicable.
            size_t len = strlen(*stdout);
            if ((*stdout)[len-1] == '\n')
                (*stdout)[len-1] = 0;
        }

    }
    return pid;
}

void execute(ast_t* tree, char** stdout) {
    // Important note; this function is only for fully resolved trees of commands.
    // If any unresolved subshells or groups exist, this function is undefined.
    // Additionally, tree must be of type AST_ROOT.
    assert(tree->type == AST_ROOT);

    // This function will eventually also perform shortest-unique-path
    // expansions. For example, typing /b/busy will resolve to /bin/busybox.

    char*  prog;
    char** argv = malloc_trap((tree->size + 1) * sizeof(char*));
    for (size_t i = 0; i < tree->size; i++) {
        ast_t* str = &((ast_t*)tree->ptr)[i];
        char *str_s = malloc_trap(str->size + 1);
        memset(str_s, 0, str->size + 1);
        memcpy(str_s, str->ptr, str->size);
        argv[i] = str_s;
    }
    argv[tree->size] = NULL;
    prog = argv[0];

    int builtin_chk = check_builtin(prog);
    if (builtin_chk != -1) {
        builtin_info[builtin_chk].func(prog, argv, stdout);
    } else {
        pid_t pid = fork_and_execvp(prog, argv, stdout);
        int wstatus;
        pid = waitpid(pid, &wstatus, 0);
    }
}
