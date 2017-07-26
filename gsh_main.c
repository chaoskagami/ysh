#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define BUF_CHUNKSIZ 64

int shell_do_exit = 0;

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
    for(size_t i=0; i < indent; i++)
        printf("  ");

    if (ast->type == AST_ROOT) {
        printf("root [%ld]\n", ast->size);
        for(size_t id = 0; id < ast->size; id++) {
            ast_dump_print(&((ast_t*)ast->ptr)[id], indent+1);
        }
    } else if (ast->type == AST_GRP) {
        printf("grp [%ld]\n", ast->size);
        for(size_t id = 0; id < ast->size; id++) {
            ast_dump_print(&((ast_t*)ast->ptr)[id], indent+1);
        }
    } else if (ast->type == AST_STR) {
        printf("str '");
        char * str = ast->ptr;
        size_t max = ast->size;
        for(size_t s = 0; s < max; s++)
            printf("%c", str[s]);
        printf("'\n");
    } else {
        printf("?\n");
    }
}

ast_t* parse(char* data) {
    ast_t *ast = malloc_trap(sizeof(ast_t));
    ast->type = AST_ROOT;
    split_line(data, strlen(data), (ast_t**)&ast->ptr, &ast->size, 0);

    ast_t *ptr = ast->ptr;

    ast_dump_print(ast, 0);

    return ast;
}

int main(int c, char *v[]) {
    while (!shell_do_exit) {
        // Read a command in.
        char *input  = read_input();
        ast_t *toks  = parse(input);
        free(toks);
        free(input);
    }
}
