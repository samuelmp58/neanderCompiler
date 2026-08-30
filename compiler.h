#ifndef COMPILER_H
#define COMPILER_H

#include "vec.h"
#include <stdbool.h>
#include <stdio.h>

#define MAX_VARS 32

typedef enum {
  eof,
  none,
  comma, // ,
  semi,  // ;
  equ,   // =
  err,
  operation, // + - * /
  number,
  var,
  id,
  colon,
  parentheses, // ()
  curly_brackets,
  print,
  _if,
  _while,
  op_rel // == != < <= > >=
} TokenTypes;

typedef struct {
  TokenTypes type;
  int val;
  char name[32];
} Token;

/* Protótipos das Funções */
void compiler_init(void);
void compiler_cleanup(void);
void compile_statement(Token *tk, FILE *file);
bool is_end_token(TokenTypes current, const TokenTypes *end_tokens);
void getExpression(Token *tk, FILE *file, const TokenTypes *end_tokens,
                   char *out_expr, size_t out_size);
Token syntax(FILE *f);
void add_var(Token tk);
void compile(FILE *file);
extern FILE *file_out;

#endif // COMPILER_H
