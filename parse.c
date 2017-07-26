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
#include "flag_vals.h"

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

void split_line(char* line, size_t len, ast_t** ast, size_t* siz, int mode) {
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

ast_t* parse(char* data) {
    ast_t *ast = malloc_trap(sizeof(ast_t));
    ast->type = AST_ROOT;
    split_line(data, strlen(data), (ast_t**)&ast->ptr, &ast->size, 0);

    ast_t *ptr = ast->ptr;

    if (obscene_debug) ast_dump_print(ast, 0);

    return ast;
}

void expand_vars(ast_t* ast) {
    assert(ast->type == AST_STR);

    char *ptr_old = (char*) ast->ptr;
    size_t ptr_sz = ast->size;
    size_t new_sz = 0;
    size_t v_at = 0, v_sz = 0;
    char *new = NULL, *var = NULL;

    int mode = 0;
    for(size_t i=0; i < ptr_sz && ptr_old[i] != 0; i++) {
        switch (ptr_old[i]) {
            case '$':
                i++;
                v_at = i;
                while((isalpha(ptr_old[i]) || ptr_old[i] == '_') &&
                      i < ptr_sz && ptr_old[i] != 0)
                    i++;
                v_sz = i - v_at;
                if (ptr_old[i] != '$') {
                    i--;
                }
                var = malloc_trap(v_sz + 1);
                memset(var, 0, v_sz + 1);
                memcpy(var, &ptr_old[v_at], v_sz);

                // TODO - get map for $var and insert to output

                break;
            default:
                new_sz++;
                new = realloc_trap(new, new_sz);
                new[new_sz-1] = ptr_old[i];
                break;
        }
    }

    ast->ptr = new;
    ast->size = new_sz;

//    printf("(%ld) %s", ast->size, (char*)ast->ptr);
}

void ast_resolve_subs(ast_t* ast, int master) {
    for(size_t id = 0; id < ast->size; id++) {
        ast_t* chk = &((ast_t*)ast->ptr)[id];
        if (chk->type == AST_ROOT || chk->type == AST_GRP)
            ast_resolve_subs(chk, 0);
        // If needed, expand variables in strings.
        if (chk->type == AST_STR)
            expand_vars(chk);
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
