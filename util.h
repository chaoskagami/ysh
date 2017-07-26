#ifndef UTIL_H
#define UTIL_H

#define BUF_CHUNKSIZ 64

typedef int (*builtin_fn_t)(char*, char**, char**);

typedef struct {
    char name[64];
    builtin_fn_t func;
} builtin_info_t;

void* malloc_trap(size_t malloc_size);
void* realloc_trap(void *ptr, size_t malloc_size);
char *read_input();
pid_t fork_and_execvp(const char *file, char *const argv[], char** stdout);
void execute(ast_t* tree, char** stdout);

// Builtins; these are in the builtin subdir, and all must have the builtin_fn_t prototype
int builtin_chdir(char* nam, char** argv, char** stdout);

// Math builtins
int builtin_add(char* nam, char** argv, char** stdout);
int builtin_sub(char* nam, char** argv, char** stdout);
int builtin_mul(char* nam, char** argv, char** stdout);
int builtin_div(char* nam, char** argv, char** stdout);
int builtin_modulo(char* nam, char** argv, char** stdout);

#endif
