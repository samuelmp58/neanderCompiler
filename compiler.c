#include "compiler.h"
#include "operations.h"
#include "out.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

//static int varCounter = 0;
static int lineCounter = -3;  // pra saber a linha que deu erro no bagulho
static Token *var_vec = NULL;
static int if_counter = 0;    // pra poder diferenciar o nome da label na hora do jmp
static int while_counter = 0; // mesma coisa
FILE *file_out = NULL;

void compiler_init(void) {
  //varCounter = 0;
  lineCounter = -3;
  var_vec = (Token *)vector_create();
}

void compiler_cleanup(void) {
  if (var_vec != NULL) {
    vector_free(var_vec);
    var_vec = NULL;
  }
}

bool is_end_token(TokenTypes current, const TokenTypes *end_tokens) {
  for (int i = 0; end_tokens[i] != none; i++) {
    if (current == end_tokens[i])
      return true;
  }
  return false;
}

void getExpression(Token *tk, FILE *file, const TokenTypes *end_tokens,
                   char *out_expr, size_t out_size) {
  out_expr[0] = '\0';

  while (!is_end_token(tk->type, end_tokens) && tk->type != eof) {
    if (tk->type == number) {
      snprintf(out_expr + strlen(out_expr), out_size - strlen(out_expr), "%d",
               tk->val);
    } else if (tk->type == operation || tk->type == parentheses) {
      snprintf(out_expr + strlen(out_expr), out_size - strlen(out_expr), "%s",
               tk->name);
    } else {
      strncat(out_expr, tk->name, out_size - strlen(out_expr) - 1);
    }

    *tk = syntax(file);
  }

  out_expr[strcspn(out_expr, "\r\n")] = '\0';
}

Token syntax(FILE *f) {
  int ch = fgetc(f);

  if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
    Token tk = {operation, 0, ""};
    tk.name[0] = (char)ch;
    tk.name[1] = '\0';
    return tk;
  }

  while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
    if (ch == '\n') {
      lineCounter++;
    }
    ch = fgetc(f);
  }

  if (ch == ';')
    return (Token){semi, 0, ""};
  if (ch == ',')
    return (Token){comma, 0, ""};
  if (ch == ':')
    return (Token){colon, 0, ""};
  if (ch == EOF)
    return (Token){eof, 0, ""};

  // --- OPERADORES RELACIONAIS ---
  if (ch == '=') {
    int next = fgetc(f);
    if (next == '=')
      return (Token){op_rel, 0, "=="};
    ungetc(next, f); // caso não for, é só um '=' mesmo.
    return (Token){equ, 0, "="};
  }
  if (ch == '!') {
    int next = fgetc(f);
    if (next == '=')
      return (Token){op_rel, 0, "!="};
    ungetc(next, f);
    return (Token){err, 0, "!"};
  }
  if (ch == '<') {
    int next = fgetc(f);
    if (next == '=')
      return (Token){op_rel, 0, "<="};
    ungetc(next, f);
    return (Token){op_rel, 0, "<"};
  }
  if (ch == '>') {
    int next = fgetc(f);
    if (next == '=')
      return (Token){op_rel, 0, ">="};
    ungetc(next, f);
    return (Token){op_rel, 0, ">"};
  }

  if (ch == '(' || ch == ')') {
    Token tk = {parentheses, 0, ""};
    tk.name[0] = (char)ch;
    tk.name[1] = '\0';
    return tk;
  }

  if (ch == '{' || ch == '}') {
    Token tk = {curly_brackets, 0, ""};
    tk.name[0] = (char)ch;
    tk.name[1] = '\0';
    return tk;
  }

  if (isdigit(ch)) {
    ungetc(ch, f);
    int num;
    fscanf(f, "%d", &num);
    return (Token){number, num, ""};
  }

  if (isalpha(ch)) {
    char buffer[32];
    int i = 0;

    while (isalnum(ch) && i < 31) {
      buffer[i++] = ch;
      ch = fgetc(f);
    }
    buffer[i] = '\0';
    ungetc(ch, f);

    if (strcmp(buffer, "var") == 0) {
      return (Token){var, 0, ""};
    } else if (strcmp(buffer, "print") == 0) {
      return (Token){print, 0, ""};
    } else if (strcmp(buffer, "if") == 0) {
      return (Token){_if, 0, ""};
    }else if (strcmp(buffer, "while") == 0) {
	  return (Token){_while, 0, ""};
	}else {
      Token tk;
      tk.type = id;
      tk.val = 0;
      strcpy(tk.name, buffer);
      return tk;
    }
  }

  return (Token){err, 0, ""};
}

static void eval_and_store(const char *expr, const char *target) {
  if (expr[0] == '\0')
    return;

  bool is_num = true;
  for (int i = 0; expr[i] != '\0'; i++) {
    if (!isdigit(expr[i])) {
      is_num = false;
      break;
    }
  }

  bool is_id = true;
  for (int i = 0; expr[i] != '\0'; i++) {
    if (!isalnum(expr[i]) && expr[i] != '_') {
      is_id = false;
      break;
    }
  }

  char buffer[128];
  if (is_num) {
    snprintf(buffer, sizeof(buffer), "\tldi %s\n\tsta %s\n", expr, target);
    emit_fmt(SECTION_MAIN, buffer);
  } else if (is_id) {
    snprintf(buffer, sizeof(buffer), "\tlda %s\n\tsta %s\n", expr, target);
    emit_fmt(SECTION_MAIN, buffer);
  } else {
    process_operation(expr, target, false);
  }
}

// Emite a instrução de salto caso a condição do IF seja FALSA
static int cond_skip_counter = 0;

static void emit_conditional_jump(const char *op, const char *lbl_false) {
  char buffer[256];

  if (strcmp(op, "==") == 0) {
    snprintf(buffer, sizeof(buffer), "\tlda _tc1\n\tsub _tc2\n\tjnz %s\n",
             lbl_false);
  } else if (strcmp(op, "!=") == 0) {
    snprintf(buffer, sizeof(buffer), "\tlda _tc1\n\tsub _tc2\n\tjz %s\n",
             lbl_false);
  } else if (strcmp(op, ">=") == 0) {
    snprintf(buffer, sizeof(buffer), "\tlda _tc1\n\tsub _tc2\n\tjn %s\n",
             lbl_false);
  } else if (strcmp(op, "<=") == 0) {
    snprintf(buffer, sizeof(buffer), "\tlda _tc2\n\tsub _tc1\n\tjn %s\n",
             lbl_false);
  } else if (strcmp(op, "<") == 0) {
    int id_skip = cond_skip_counter++;
    snprintf(buffer, sizeof(buffer),
             "\tlda _tc1\n"
             "\tsub _tc2\n"
             "\tjn _skip_false_%d\n"
             "\tjmp %s\n"
             "\n_skip_false_%d:\n",
             id_skip, lbl_false, id_skip);
  } else if (strcmp(op, ">") == 0) {
    int id_skip = cond_skip_counter++;
    snprintf(buffer, sizeof(buffer),
             "\tlda _tc2\n"
             "\tsub _tc1\n"
             "\tjn _skip_false_%d\n"
             "\tjmp %s\n"
             "\n_skip_false_%d:\n",
             id_skip, lbl_false, id_skip);
  } else {
    snprintf(buffer, sizeof(buffer), "\tlda _tc1\n\tsub _tc2\n\tjnz %s\n",
             lbl_false);
  }

  emit_fmt(SECTION_MAIN, buffer);
}

void compile_statement(Token *tk, FILE *file) {
  // ==========================================================
  // TRATAMENTO DE PRINT
  // ==========================================================
  if (tk->type == print) {
    *tk = syntax(file);
    char buffer[256];
    if (tk->type == id) {
      snprintf(buffer, sizeof(buffer), "\tlda %s\n\tout 0\n", tk->name);
    } else if (tk->type == number) {
      snprintf(buffer, sizeof(buffer), "\tldi %d\n\tout 0\n", tk->val);
    } else {
      printf(
          "erro linha %d -> Esperado variavel ou inteiro como argumento...\n",
          lineCounter);
      return;
    }
    emit_fmt(SECTION_MAIN, buffer);
    *tk = syntax(file);
  }

  // ==========================================================
  // TRATAMENTO DE IF / ELSE IF / ELSE
  // ==========================================================
  else if (tk->type == _if) {
    int current_if = if_counter++;
    int elseif_counter = 0;

    char lbl_end[32];
    snprintf(lbl_end, sizeof(lbl_end), "_if_end_%d", current_if);

    char lbl_next[32];
    snprintf(lbl_next, sizeof(lbl_next), "_elseif_%d_%d", current_if,
             elseif_counter);

    // --- PRIMEIRO IF ---
    *tk = syntax(file); // if
    if (tk->type != parentheses || strcmp(tk->name, "(") != 0) {
      printf("erro linha %d -> Esperado '(' apos 'if'...\n", lineCounter);
      return;
    }

    // Expressão Esquerda (LHS)
    *tk = syntax(file);
    char expr1[256] = "";
    getExpression(tk, file, (TokenTypes[]){semi, equ, op_rel, none}, expr1,
                  sizeof(expr1));
    eval_and_store(expr1, "_tc1");

    // Operador Relacional ou Atribuição
    char op[4] = "==";
    if (tk->type == op_rel) {
      strncpy(op, tk->name, sizeof(op) - 1);
      op[sizeof(op) - 1] = '\0';
      *tk = syntax(file);
    } else if (tk->type == equ) {
      *tk = syntax(file);
      if (tk->type == equ) {
        strcpy(op, "==");
        *tk = syntax(file);
      } else {
        strcpy(op, "==");
      }
    } else {
      printf(
          "erro linha %d -> Esperado operador relacional para comparacao...\n",
          lineCounter);
      return;
    }

    // Expressão Direita
    char expr2[256] = "";
    getExpression(tk, file, (TokenTypes[]){parentheses, none}, expr2,
                  sizeof(expr2));
    eval_and_store(expr2, "_tc2");

    if (strcmp(tk->name, ")") == 0) {
      *tk = syntax(file);
    }

    // Comentário do Teste da Condição
    char buffer[570];
    snprintf(buffer, sizeof(buffer),
             "\n\t; --- Teste da Condicao (%s %s %s) do IF #%d ---\n", expr1,
             op, expr2, current_if);
    emit_fmt(SECTION_MAIN, buffer);

    // Salto Condicional
    emit_conditional_jump(op, lbl_next);

    // Comentário do Corpo
    snprintf(buffer, sizeof(buffer), "\t; --- Inicio do Corpo do IF #%d ---\n",
             current_if);
    emit_fmt(SECTION_MAIN, buffer);

    // Corpo do IF
    if (strcmp(tk->name, "{") == 0) {
      *tk = syntax(file);
    }

    while (strcmp(tk->name, "}") != 0 && tk->type != eof) {
      compile_statement(tk, file);
    }

    // Salto para o fim da estrutura de decisão
    snprintf(buffer, sizeof(buffer), "\tjmp %s\n", lbl_end);
    emit_fmt(SECTION_MAIN, buffer);

    // --- CADEIA ELSE IF / ELSE ---
    *tk = syntax(file); // Avança após a '}' do IF

    while (tk->type == id && strcmp(tk->name, "else") == 0) {
      *tk = syntax(file); // Consome 'else'

      if (tk->type == _if) {
        // === ELSE IF ===
        elseif_counter++;

        snprintf(buffer, sizeof(buffer),
                 "\n%s:\n\t; --- Teste ELSE IF #%d_%d ---\n", lbl_next,
                 current_if, elseif_counter);
        emit_fmt(SECTION_MAIN, buffer);

        snprintf(lbl_next, sizeof(lbl_next), "_elseif_%d_%d", current_if,
                 elseif_counter);

        *tk = syntax(file); // Consome 'if'
        if (strcmp(tk->name, "(") != 0) {
          printf("erro linha %d -> Esperado '(' apos 'else if'...\n",
                 lineCounter);
          return;
        }

        // LHS
        *tk = syntax(file);
        expr1[0] = '\0';
        getExpression(tk, file, (TokenTypes[]){semi, equ, op_rel, none}, expr1,
                      sizeof(expr1));
        eval_and_store(expr1, "_tc1");

        // Operador Relacional
        op[0] = '\0';
        if (tk->type == op_rel) {
          strncpy(op, tk->name, sizeof(op) - 1);
          op[sizeof(op) - 1] = '\0';
          *tk = syntax(file);
        } else if (tk->type == equ) {
          *tk = syntax(file);
          if (tk->type == equ) {
            strcpy(op, "==");
            *tk = syntax(file);
          } else {
            strcpy(op, "==");
          }
        }

        // RHS
        expr2[0] = '\0';
        getExpression(tk, file, (TokenTypes[]){parentheses, none}, expr2,
                      sizeof(expr2));
        eval_and_store(expr2, "_tc2");

        if (strcmp(tk->name, ")") == 0)
          *tk = syntax(file);

        emit_conditional_jump(op, lbl_next);

        if (strcmp(tk->name, "{") == 0)
          *tk = syntax(file);
        while (strcmp(tk->name, "}") != 0 && tk->type != eof) {
          compile_statement(tk, file);
        }

        snprintf(buffer, sizeof(buffer), "\tjmp %s\n", lbl_end);
        emit_fmt(SECTION_MAIN, buffer);

        *tk = syntax(file);

      } else {
        // === ELSE ===
        snprintf(buffer, sizeof(buffer),
                 "\n%s:\n\t; --- Inicio do Corpo do ELSE #%d ---\n", lbl_next,
                 current_if);
        emit_fmt(SECTION_MAIN, buffer);

        if (strcmp(tk->name, "{") == 0)
          *tk = syntax(file);
        while (strcmp(tk->name, "}") != 0 && tk->type != eof) {
          compile_statement(tk, file);
        }

        lbl_next[0] = '\0';
        *tk = syntax(file);
        break;
      }
    }

    if (lbl_next[0] != '\0') {
      snprintf(buffer, sizeof(buffer), "\n%s:\n", lbl_next);
      emit_fmt(SECTION_MAIN, buffer);
    }

    snprintf(buffer, sizeof(buffer),
             "\n%s:\n\t; --- Fim da Estrutura IF #%d ---\n", lbl_end,
             current_if);
    emit_fmt(SECTION_MAIN, buffer);
  }

  // ==========================================================
  // TRATAMENTO DE VAR
  // ==========================================================
  else if (tk->type == var) {
    *tk = syntax(file); // var
    if (tk->type != id) {
      printf("erro linha %d -> Esperado o nome da variavel...\n", lineCounter);
      return;
    }

	// Guardando o nome
    char varName[32];
    strcpy(varName, tk->name);
    Token varToken = *tk; 

    *tk = syntax(file); // = ou ;

    while (tk->type != semi && tk->type != eof) {
      if (tk->type == equ || tk->type == comma) {
        if (tk->type == equ) {
          *tk = syntax(file);

          if (tk->type != number && tk->type != parentheses && tk->type != id) {
            printf(
                "erro linha %d -> Esperado um valor inteiro para variavel...\n",
                lineCounter);
            return;
          }

          char expr[256] = "";
          getExpression(tk, file, (TokenTypes[]){semi, comma, none}, expr,
                        sizeof(expr));
          printf("setando %s = %s\n", varToken.name, expr);
          process_operation(expr, varToken.name, true);

          if (tk->type == comma) {
            *tk = syntax(file);
          } else {
            break;
          }
        } else {
          if (varToken.type == id) {
            printf("Criando variavel NULL %s \n", varToken.name);
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s: db 0\n", varToken.name);
            emit_fmt(SECTION_VARS, buffer);
            *tk = syntax(file);
          }
        }
        varToken = *tk;
        *tk = syntax(file);
      } else {
        printf(
            "erro linha %d -> Esperado '=' ou ',' após nome de variavel...\n",
            lineCounter);
        return;
      }
    }
    if (tk->type == semi)
      *tk = syntax(file);
  }
  
  // ==========================================================
  // TRATAMENTO DE WHILE
  // ==========================================================
  else if (tk->type == _while) {
    int current_while = while_counter++;

    char lbl_start[32];
    char lbl_end[32];
    snprintf(lbl_start, sizeof(lbl_start), "_while_start_%d", current_while);
    snprintf(lbl_end, sizeof(lbl_end), "_while_end_%d", current_while);

    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "\n%s:\n\t; --- Teste da Condicao do WHILE #%d ---\n", lbl_start,
             current_while);
    emit_fmt(SECTION_MAIN, buffer);

    *tk = syntax(file); // while
    if (tk->type != parentheses || strcmp(tk->name, "(") != 0) {
      printf("erro linha %d -> Esperado '(' apos 'while'...\n", lineCounter);
      return;
    }

    // Lado esquerdo da condição -> _tc1
    *tk = syntax(file);
    char expr1[256] = "";
    getExpression(tk, file, (TokenTypes[]){semi, equ, op_rel, none}, expr1,
                  sizeof(expr1));
    eval_and_store(expr1, "_tc1");

    // Identificando operador
    char op[4] = "==";
    if (tk->type == op_rel) {
      strncpy(op, tk->name, sizeof(op) - 1);
      op[sizeof(op) - 1] = '\0';
      *tk = syntax(file);
    } else if (tk->type == equ) {
      *tk = syntax(file);
      if (tk->type == equ) {
        strcpy(op, "==");
        *tk = syntax(file);
      } else {
        strcpy(op, "==");
      }
    } else {
      printf("erro linha %d -> Operador invalido na condicao do while...\n",
             lineCounter);
      return;
    }

    // lado direito -> _tc2
    char expr2[256] = "";
    getExpression(tk, file, (TokenTypes[]){parentheses, none}, expr2,
                  sizeof(expr2));
    eval_and_store(expr2, "_tc2");

    if (strcmp(tk->name, ")") == 0) {
      *tk = syntax(file);
    }

    // Se for FALSE, desvia para o FIM do loop (_while_end_X)
    emit_conditional_jump(op, lbl_end);

    snprintf(buffer, sizeof(buffer),
             "\t; --- Inicio do Corpo do WHILE #%d ---\n", current_while);
    emit_fmt(SECTION_MAIN, buffer);

    if (strcmp(tk->name, "{") == 0) {
      *tk = syntax(file);
    }

    // Processa o corpo do loop recursivamente
    while (strcmp(tk->name, "}") != 0 && tk->type != eof) {
      compile_statement(tk, file);
    }

    // Salta incondicionalmente de volta para o início para reavaliar a condição
    snprintf(buffer, sizeof(buffer),
             "\tjmp %s\n\n%s:\n\t; --- Fim do WHILE #%d ---\n", lbl_start,
             lbl_end, current_while);
    emit_fmt(SECTION_MAIN, buffer);

    *tk = syntax(file); // }
  }

  else {
    *tk = syntax(file);
  }
}

void compile(FILE *file) {
  Token tk = syntax(file);

  while (tk.type != eof) {
    compile_statement(&tk, file);
  }
}
