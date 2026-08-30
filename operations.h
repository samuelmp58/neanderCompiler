#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum { NODE_VALUE, NODE_UNARY, NODE_BINARY } NodeType;

typedef struct AST {
  NodeType type;
  char op;
  char *value;
  struct AST *left;
  struct AST *right;
  char temp_name[16];
} AST;

typedef struct {
  const char *input;
  size_t position;
  int error;
} Parser;

// void process_operation(const char *expression, const char *var_name);
void process_operation(const char *expression, const char *var_name,
                       bool declare_in_vars);
void destroy_ast(AST *node);
void print_ast(const AST *node);

#endif // OPERATIONS_H
