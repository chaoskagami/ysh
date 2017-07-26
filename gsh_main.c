#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_CHUNKSIZ 64

// Exit the main interactive loop
int shell_do_exit = 0;

// Various flags.
int obscene_debug = 0;

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

// Unset. Not used in practice.
#define AST_UNSET 0
// Root element split to parameters.
#define AST_ROOT 1
// A fixed size string; ptr should be a char*
#define AST_STR  2
// Multiple elements which must be concatentated together to form a complete string.
#define AST_GRP  3

// Suppose the following input:
//   echo $(printf %x $(echo 42)) "hi world"

// The parse tree should be:

// AST_ROOT
//   AST_STR "echo"
//   AST_ROOT
//     AST_STR "printf"
//     AST_STR "%x"
//     AST_ROOT
//       AST_STR "echo"
//       AST_STR "42"
//   AST_STR "hi world"

// In order to evaluate this, all AST_ROOT subelements must be
// evaluated such that no AST_ROOT elements remain aside from the tree's
// primary element.

// Postnote; implementation is a bit different as one can see in --obscene
// mode.

typedef struct ast_s {
    int    type;
    size_t size; // Number of elements for AST_ROOT|AST_GRP,
                 // and number of characters for AST_STR
    void*  ptr;  // either ast_s* or char*
} ast_t;

void split_line(char* line, size_t len, ast_t** ast, size_t* siz, int mode) {
#if 0
    printf("split(%ld): ", len);
    for(size_t i=0; i < len; i++) {
        printf("%c", line[i]);
    }
    printf("\n");
#endif
    ast_t *ast_grp = malloc_trap(sizeof(ast_t));
    ast_grp[0].type = AST_UNSET;
    ast_t *group;
    size_t count = 0;
    size_t ss_count = 0, ss_size = 0;
    size_t in_q = 0, q_size = 0;
    char *ss_str, *q_str;
    int last = AST_UNSET;
    for(size_t i=0; line[i] != 0, i < len; i++) {
        switch(line[i]) {
            case '\'':
                if (ss_count || in_q || mode == 1) break;
                // We're in a single quote. No escapes are allowed,
                // nor subshells. Therefore, we'll simply seek to the
                // next single quote available and insert it as AST_STR.
                ++i;
                last = ast_grp[count].type = AST_STR;
                ast_grp[count].ptr  = &line[i];
                ast_grp[count].size = i; // Temporary save.
                while(line[i] != '\'' && i < len) {
                    if (line[i] == 0) {
                        printf("syntax error: unclosed single quote\n");
                        free(ast_grp);
                        return;
                    }
                    ++i;
                }
                ast_grp[count].size = (i - ast_grp[count].size);
                ++count;
                ast_grp = realloc_trap(ast_grp, sizeof(ast_t) * (count+1));
                ast_grp[count].type = AST_UNSET;
                break;
            case '"': // Quotes
                if (ss_count || mode == 1) break;
                if (!in_q) {
                    in_q = 1; // Quote begin.
                    last = ast_grp[count].type = AST_GRP;
                    q_str = &line[i+1];
                } else {
                    in_q = 0; // Quote end.
                    q_size = &line[i-1] - q_str + 1;

                    split_line(q_str, q_size, (ast_t**)&ast_grp[count].ptr, &ast_grp[count].size, 1);

                    ++count;
                    ast_grp = realloc_trap(ast_grp, sizeof(ast_t) * (count+1));
                    ast_grp[count].type = AST_UNSET;
                }
                break;
            case '{': // Subshell begin.
                ss_count++;
                if (in_q) break;
                if (ss_count == 1) {
                    if (mode == 1 && ast_grp[count].type != AST_UNSET) {
                        ++count;
                        ast_grp = realloc_trap(ast_grp, sizeof(ast_t) * (count+1));
                    }
                    // The first open brace.
                    last = ast_grp[count].type = AST_ROOT;
                    ss_str  = &line[i+1];
                }
                break;
            case '}': // Subshell end
                if (ss_count) {
                    ss_count--;
                    if (in_q) break;
                    if (ss_count == 0) {
                        // Tis the end of the subshell.
                        ss_size = &line[i-1] - ss_str + 1;

                        // And now, for something different; we need to run this
                        // function recursively over the subshell string.
                        split_line(ss_str, ss_size, (ast_t**)&ast_grp[count].ptr, &ast_grp[count].size, 0);

                        ++count;
                        ast_grp = realloc_trap(ast_grp, sizeof(ast_t) * (count+1));
                        ast_grp[count].type = AST_UNSET;
                        if (mode == 1) ++i;
                    }
                }
                break;
            case ' ':
            case '\n':
            case '\t':
                break;
            default:
                if (ss_count || in_q || mode == 1) break;
                if ((i && isspace(line[i-1])) || i == 0) {
                    last = ast_grp[count].type = AST_STR;
                    ast_grp[count].ptr  = &line[i];
                    ast_grp[count].size = i; // Temporary save.
                    while(!isspace(line[i]) && i < len) {
                        if (line[i] == 0) {
                            i--;
                            break;
                        }
                        ++i;
                    }
                    ast_grp[count].size = (i - ast_grp[count].size);
                    ++count;
                    ast_grp = realloc_trap(ast_grp, sizeof(ast_t) * (count+1));
                    ast_grp[count].type = AST_UNSET;
                }
                break;
        }
        if (mode == 1 && ss_count == 0 && i < len) {
            if (ast_grp[count].type == AST_UNSET) {
                last = ast_grp[count].type = AST_STR;
                ast_grp[count].ptr  = &line[i];
                ast_grp[count].size = 0; // Temporary save.
            }
            if (ast_grp[count].type == AST_STR) {
                ast_grp[count].size++; // Temporary save.
            }
        }
    }

    if (mode == 1 && last == AST_STR) ++count;

    if (ss_count)
        printf("warn: unterminated subshell\n");

    *ast = ast_grp;
    *siz = count;
}

void ast_dump_print(ast_t* ast, size_t indent) {
    if (ast->type == AST_ROOT) {
        for(size_t i=0; i < indent; i++)
            printf("  ");

        printf("root [%ld]\n", ast->size);
        for(size_t id = 0; id < ast->size; id++) {
            ast_dump_print(&((ast_t*)ast->ptr)[id], indent+1);
        }
    } else if (ast->type == AST_GRP) {
        for(size_t i=0; i < indent; i++)
            printf("  ");

        printf("grp [%ld]\n", ast->size);
        for(size_t id = 0; id < ast->size; id++) {
            ast_dump_print(&((ast_t*)ast->ptr)[id], indent+1);
        }
    } else if (ast->type == AST_STR) {
        for(size_t i=0; i < indent; i++)
            printf("  ");

        printf("str '");
        char * str = ast->ptr;
        size_t max = ast->size;
        for(size_t s = 0; s < max; s++)
            printf("%c", str[s]);
        printf("'\n");
    } else {
        assert(0);
    }
}

ast_t* parse(char* data) {
    ast_t *ast = malloc_trap(sizeof(ast_t));
    ast->type = AST_ROOT;
    split_line(data, strlen(data), (ast_t**)&ast->ptr, &ast->size, 0);

    ast_t *ptr = ast->ptr;

    if (obscene_debug) ast_dump_print(ast, 0);

    return ast;
}

#define BUFFER

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

    pid_t pid = fork_and_execvp(prog, argv, stdout);
    int wstatus;
    pid = waitpid(pid, &wstatus, 0);
}

void ast_resolve_subs(ast_t* ast, int master) {
    for(size_t id = 0; id < ast->size; id++) {
        ast_t* chk = &((ast_t*)ast->ptr)[id];
        if (chk->type == AST_ROOT || chk->type == AST_GRP)
            ast_resolve_subs(chk, 0);
    }

    if (obscene_debug) ast_dump_print(ast, 0);

    // No more AST_ROOT or AST_GRP left to fix up. Now, depending
    // on type, we need to do the following:
    // for AST_ROOT: execute and capture stdout, then use stdout as new AST_STR
    // for AST_GRP: concatenate strings and replace AST_GRP with AST_STR

    if (master) return; // Don't fuck the tree's root node.

    if (ast->type == AST_ROOT) {
        char *output = NULL;
        execute(ast, &output);
        free(ast->ptr);
        ast->type = AST_STR;
        ast->ptr = output;
        ast->size = strlen(output);
    } else if (ast->type == AST_GRP) {
        size_t total = 0;
        size_t at = 0;
        char *buf = malloc_trap(1);
        for(size_t id = 0; id < ast->size; id++) {
            ast_t* chk = &((ast_t*)ast->ptr)[id];

            total += chk->size;
            buf = realloc_trap(buf, total);

            memcpy(&buf[at], chk->ptr, chk->size);
            at += chk->size;
        }
        free(ast->ptr);
        ast->type = AST_STR;
        ast->ptr  = buf;
        ast->size = total;
    }
}

ast_t* resolve(ast_t* tree) {
    // This traverses the tree, executing subshell commands,
    // expanding escape sequences within strings, etc
    // until only the top-level AST_ROOT remains with no more
    // expansion needed.

    ast_resolve_subs(tree, 1);

    if (obscene_debug) ast_dump_print(tree, 0);

    return tree;
}

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
    } else {
        while (!shell_do_exit) {
            // Read a command in.
            char *input  = read_input();
            ast_t *toks  = parse(input);
            toks         = resolve(toks);
            execute(toks, NULL);
            // FIXME: free the damn toks memory, this requires traversing to the bottom tho
            free(input);
        }
    }
}
