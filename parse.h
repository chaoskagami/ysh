#ifndef PARSE_H
#define PARSE_H

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

void ast_dump_print(ast_t* ast, size_t indent);
void split_line(char* line, size_t len, ast_t** ast, size_t* siz, int mode);
ast_t* parse(char* data);
void expand_vars(ast_t* ast);
void ast_resolve_subs(ast_t* ast, int master);
ast_t* resolve(ast_t* tree);

#endif
