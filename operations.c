#include "operations.h"
#include "out.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DECLARED_VARS 256

static char declared_vars[MAX_DECLARED_VARS][32];
static int declared_vars_count = 0;

static void declare_variable(const char *name, const char *init_value) {
  if (!name || name[0] == '\0')
    return;

  for (int i = 0; i < declared_vars_count; i++) {
    if (strcmp(declared_vars[i], name) == 0) {
      return;
    }
  }

  if (declared_vars_count < MAX_DECLARED_VARS) {
    strncpy(declared_vars[declared_vars_count], name, 31);
    declared_vars[declared_vars_count][31] = '\0';
    declared_vars_count++;
    emit_fmt(SECTION_VARS, "%s: db %s", name, init_value ? init_value : "0");
  }
}

// Temporarias para operações
#define MAX_TEMPS 256

static int temp_in_use[MAX_TEMPS] = {0};
static int max_temp_registered = -1;
static int label_count = 0;

static int allocate_temp(void) {
  for (int i = 0; i < MAX_TEMPS; i++) {
    if (!temp_in_use[i]) {
      temp_in_use[i] = 1;

      if (i > max_temp_registered) {
        max_temp_registered = i;
        emit_fmt(SECTION_TEMP, "_t%d: db 0", i);
      }
      return i;
    }
  }
  fprintf(stderr, "Erro: Limite de temporarias excedido.\n");
  return -1;
}

static void free_temp(int id) {
  if (id >= 0 && id < MAX_TEMPS) {
    temp_in_use[id] = 0;
  }
}

static void free_temp_by_name(const char *name) {
  if (!name)
    return;
  int id;
  if (sscanf(name, "_t%d", &id) == 1) {
    free_temp(id);
  }
}

static void reset_all_temps(void) {
  memset(temp_in_use, 0, sizeof(temp_in_use));
}

// Auxiliares
static int is_variable(const char *str) {
  if (!str || str[0] == '\0')
    return 0;
  return isalpha((unsigned char)str[0]) || str[0] == '_';
}

static void skip_spaces(Parser *parser) {
  while (isspace((unsigned char)parser->input[parser->position])) {
    parser->position++;
  }
}

static char peek(Parser *parser) {
  skip_spaces(parser);
  return parser->input[parser->position];
}

static int consume(Parser *parser, char expected) {
  if (peek(parser) == expected) {
    parser->position++;
    return 1;
  }
  return 0;
}

static void syntax_error(Parser *parser, const char *message) {
  if (!parser->error) {
    fprintf(stderr, "Erro de sintaxe na posicao %zu: %s\n", parser->position,
            message);
  }
  parser->error = 1;
}

static char *copy_text(const char *start, size_t length) {
  char *text = malloc(length + 1);
  if (text == NULL) {
    fprintf(stderr, "Erro: Memoria insuficiente.\n");
    return NULL;
  }
  memcpy(text, start, length);
  text[length] = '\0';
  return text;
}

static AST *allocate_node(NodeType type) {
  AST *node = calloc(1, sizeof(AST));
  if (node == NULL) {
    fprintf(stderr, "Erro: Memoria insuficiente.\n");
    return NULL;
  }
  node->type = type;
  return node;
}

static AST *create_value_node(const char *start, size_t length) {
  AST *node = allocate_node(NODE_VALUE);
  if (node == NULL)
    return NULL;

  node->value = copy_text(start, length);
  if (node->value == NULL) {
    free(node);
    return NULL;
  }
  return node;
}

static AST *create_unary_node(char op, AST *right) {
  AST *node = allocate_node(NODE_UNARY);
  if (node == NULL)
    return NULL;

  node->op = op;
  node->right = right;
  return node;
}

static AST *create_binary_node(char op, AST *left, AST *right) {
  AST *node = allocate_node(NODE_BINARY);
  if (node == NULL)
    return NULL;

  node->op = op;
  node->left = left;
  node->right = right;
  return node;
}

void destroy_ast(AST *node) {
  if (node == NULL)
    return;

  destroy_ast(node->left);
  destroy_ast(node->right);

  free(node->value);
  free(node);
}

// Parser sintatico
static AST *parse_expression(Parser *parser);
static AST *parse_addition_subtraction(Parser *parser);
static AST *parse_multiplication(Parser *parser);
static AST *parse_unary(Parser *parser);
static AST *parse_power(Parser *parser);
static AST *parse_primary(Parser *parser);

static AST *parse_primary(Parser *parser) {
  skip_spaces(parser);
  char current = parser->input[parser->position];

  if (current == '(') {
    parser->position++;
    AST *node = parse_expression(parser);
    if (node == NULL)
      return NULL;

    if (!consume(parser, ')')) {
      syntax_error(parser, "esperado ')'");
      destroy_ast(node);
      return NULL;
    }
    return node;
  }

  if (isdigit((unsigned char)current) || current == '.') {
    size_t start = parser->position;
    int has_digit = 0;
    int has_dot = 0;

    while (1) {
      current = parser->input[parser->position];
      if (isdigit((unsigned char)current)) {
        has_digit = 1;
        parser->position++;
        continue;
      }
      if (current == '.' && !has_dot) {
        has_dot = 1;
        parser->position++;
        continue;
      }
      break;
    }

    if (!has_digit) {
      syntax_error(parser, "numero invalido");
      return NULL;
    }
    return create_value_node(parser->input + start, parser->position - start);
  }

  if (isalpha((unsigned char)current) || current == '_') {
    size_t start = parser->position;
    parser->position++;

    while (isalnum((unsigned char)parser->input[parser->position]) ||
           parser->input[parser->position] == '_') {
      parser->position++;
    }
    return create_value_node(parser->input + start, parser->position - start);
  }

  syntax_error(parser, "esperado numero, variavel ou '('");
  return NULL;
}

static AST *parse_power(Parser *parser) {
  AST *left = parse_primary(parser);
  if (left == NULL)
    return NULL;

  if (consume(parser, '^')) {
    AST *right = parse_unary(parser);
    if (right == NULL) {
      destroy_ast(left);
      return NULL;
    }
    AST *node = create_binary_node('^', left, right);
    if (node == NULL) {
      destroy_ast(left);
      destroy_ast(right);
      return NULL;
    }
    return node;
  }
  return left;
}

static AST *parse_unary(Parser *parser) {
  char current = peek(parser);
  if (current == '+' || current == '-') {
    parser->position++;
    AST *right = parse_unary(parser);
    if (right == NULL)
      return NULL;

    AST *node = create_unary_node(current, right);
    if (node == NULL) {
      destroy_ast(right);
      return NULL;
    }
    return node;
  }
  return parse_power(parser);
}

static AST *parse_multiplication(Parser *parser) {
  AST *left = parse_unary(parser);
  if (left == NULL)
    return NULL;

  while (1) {
    char current = peek(parser);
    if (current != '*' && current != '/' && current != '%')
      break;

    parser->position++;
    AST *right = parse_unary(parser);
    if (right == NULL) {
      destroy_ast(left);
      return NULL;
    }
    AST *node = create_binary_node(current, left, right);
    if (node == NULL) {
      destroy_ast(left);
      destroy_ast(right);
      return NULL;
    }
    left = node;
  }
  return left;
}

static AST *parse_addition_subtraction(Parser *parser) {
  AST *left = parse_multiplication(parser);
  if (left == NULL)
    return NULL;

  while (1) {
    char current = peek(parser);
    if (current != '+' && current != '-')
      break;

    parser->position++;
    AST *right = parse_multiplication(parser);
    if (right == NULL) {
      destroy_ast(left);
      return NULL;
    }
    AST *node = create_binary_node(current, left, right);
    if (node == NULL) {
      destroy_ast(left);
      destroy_ast(right);
      return NULL;
    }
    left = node;
  }
  return left;
}

static AST *parse_expression(Parser *parser) {
  return parse_addition_subtraction(parser);
}

void print_ast(const AST *node) {
  if (node == NULL)
    return;

  if (node->type == NODE_VALUE) {
    printf("%s", node->value);
    return;
  }
  if (node->type == NODE_UNARY) {
    printf("(%c", node->op);
    print_ast(node->right);
    printf(")");
    return;
  }
  if (node->type == NODE_BINARY) {
    printf("(");
    print_ast(node->left);
    printf(" %c ", node->op);
    print_ast(node->right);
    printf(")");
  }
}

// Gerador de código "Assembly"
static void load_operand(AST *node, char *loc_out, size_t loc_size) {
  if (node->type == NODE_VALUE) {
    int id = allocate_temp();
    snprintf(loc_out, loc_size, "_t%d", id);

    if (is_variable(node->value)) {
      declare_variable(node->value, "0");
      emit_fmt(SECTION_MAIN, "\tlda %s", node->value);
    } else {
      emit_fmt(SECTION_MAIN, "\tldi %s", node->value);
    }
    emit_fmt(SECTION_MAIN, "\tsta %s", loc_out);
  } else {
    strncpy(loc_out, node->temp_name, loc_size);
  }
}

static void generate_ast_code(AST *node) {
  if (node == NULL)
    return;

  if (node->type == NODE_VALUE) {
    if (is_variable(node->value)) {
      declare_variable(node->value, "0");
    }
    return;
  }

  generate_ast_code(node->left);
  generate_ast_code(node->right);

  if (node->type == NODE_UNARY) {
    char loc_right[16];
    load_operand(node->right, loc_right, sizeof(loc_right));

    int res_id = allocate_temp();
    snprintf(node->temp_name, sizeof(node->temp_name), "_t%d", res_id);

    if (node->op == '-') {
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tnot");
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tldi 1");
      emit_fmt(SECTION_MAIN, "\tadd %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
    } else {
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
    }

    free_temp_by_name(loc_right);
    return;
  }

  if (node->type == NODE_BINARY) {
    char loc_left[16], loc_right[16];
    load_operand(node->left, loc_left, sizeof(loc_left));
    load_operand(node->right, loc_right, sizeof(loc_right));

    int res_id = allocate_temp();
    snprintf(node->temp_name, sizeof(node->temp_name), "_t%d", res_id);

    switch (node->op) {
    case '+':
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_left);
      emit_fmt(SECTION_MAIN, "\tadd %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      break;

    case '-':
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tnot");
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tldi 1");
      emit_fmt(SECTION_MAIN, "\tadd %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tadd %s", loc_left);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      break;

    case '*': {
      int id_lbl = label_count++;
      int cnt_id = allocate_temp();
      char loc_cnt[16];
      snprintf(loc_cnt, sizeof(loc_cnt), "_t%d", cnt_id);

      emit_fmt(SECTION_MAIN, "\tldi 0");
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);

      emit_fmt(SECTION_MAIN, "\tlda %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tsta %s", loc_cnt);

      emit_fmt(SECTION_MAIN, "loop_mult_%d:", id_lbl);
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_cnt);
      emit_fmt(SECTION_MAIN, "\tjz end_mult_%d", id_lbl);

      emit_fmt(SECTION_MAIN, "\tldi 255");
      emit_fmt(SECTION_MAIN, "\tadd %s", loc_cnt);
      emit_fmt(SECTION_MAIN, "\tsta %s", loc_cnt);

      emit_fmt(SECTION_MAIN, "\tlda %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tadd %s", loc_left);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);

      emit_fmt(SECTION_MAIN, "\tjmp loop_mult_%d", id_lbl);
      emit_fmt(SECTION_MAIN, "end_mult_%d:", id_lbl);

      free_temp(cnt_id);
      break;
    }

    case '/': {
      int id_lbl = label_count++;
      int rem_id = allocate_temp();
      char loc_rem[16];
      snprintf(loc_rem, sizeof(loc_rem), "_t%d", rem_id);

      emit_fmt(SECTION_MAIN, "\tldi 0");
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_left);
      emit_fmt(SECTION_MAIN, "\tsta %s", loc_rem);

      emit_fmt(SECTION_MAIN, "loop_div_%d:", id_lbl);
      emit_fmt(SECTION_MAIN, "\tlda %s", loc_right);
      emit_fmt(SECTION_MAIN, "\tnot");
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tldi 1");
      emit_fmt(SECTION_MAIN, "\tadd %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tadd %s", loc_rem);
      emit_fmt(SECTION_MAIN, "\tjn end_div_%d", id_lbl);

      emit_fmt(SECTION_MAIN, "\tsta %s", loc_rem);
      emit_fmt(SECTION_MAIN, "\tlda %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tldi 1");
      emit_fmt(SECTION_MAIN, "\tadd %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tsta %s", node->temp_name);
      emit_fmt(SECTION_MAIN, "\tjmp loop_div_%d", id_lbl);
      emit_fmt(SECTION_MAIN, "end_div_%d:", id_lbl);

      free_temp(rem_id);
      break;
    }
    }

    free_temp_by_name(loc_left);
    free_temp_by_name(loc_right);
  }
}

void process_operation(const char *expression, const char *var_name,
                       bool declare_in_vars) {
  if (expression == NULL || expression[0] == '\0') {
    fprintf(stderr, "Erro: Expressao vazia.\n");
    return;
  }

  Parser parser = {.input = expression, .position = 0, .error = 0};
  AST *root = parse_expression(&parser);

  if (root == NULL || parser.error) {
    destroy_ast(root);
    return;
  }

  if (peek(&parser) != '\0') {
    syntax_error(&parser, "caractere inesperado");
    destroy_ast(root);
    return;
  }

  if (root->type == NODE_VALUE) {
    if (var_name && var_name[0] != '\0') {
      if (is_variable(root->value)) {

        if (declare_in_vars) {
          declare_variable(var_name, "0");
          declare_variable(root->value, "0");
        }
        emit_fmt(SECTION_MAIN, "\tlda %s", root->value);
        emit_fmt(SECTION_MAIN, "\tsta %s", var_name);
      } else {
        if (declare_in_vars) {
          declare_variable(var_name, root->value);
        }
      }
    }
  } else {
    generate_ast_code(root);

    if (var_name && var_name[0] != '\0') {
      if (declare_in_vars) {
        declare_variable(var_name, "0");
      }
      emit_fmt(SECTION_MAIN, "\tlda %s", root->temp_name);
      emit_fmt(SECTION_MAIN, "\tsta %s", var_name);
    }

    reset_all_temps();
  }

  destroy_ast(root);
}
