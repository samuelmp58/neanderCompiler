#include "out.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUFFER 8192

static char vars_buffer[MAX_BUFFER];
static char temp_buffer[MAX_BUFFER];
static char main_buffer[MAX_BUFFER];

void init_output(void) {
  vars_buffer[0] = '\0';
  temp_buffer[0] = '\0';
  main_buffer[0] = '\0';
}

void emit_fmt(SectionType section, const char *format, ...) {
  char temp[256];
  va_list args;

  va_start(args, format);
  vsnprintf(temp, sizeof(temp), format, args);
  va_end(args);

  switch (section) {
  case SECTION_VARS:
    strncat(vars_buffer, temp, MAX_BUFFER - strlen(vars_buffer) - 1);
    strncat(vars_buffer, "\n", MAX_BUFFER - strlen(vars_buffer) - 1);
    break;
  case SECTION_TEMP:
    strncat(temp_buffer, temp, MAX_BUFFER - strlen(temp_buffer) - 1);
    strncat(temp_buffer, "\n", MAX_BUFFER - strlen(temp_buffer) - 1);
    break;
  case SECTION_MAIN:
    strncat(main_buffer, temp, MAX_BUFFER - strlen(main_buffer) - 1);
    strncat(main_buffer, "\n", MAX_BUFFER - strlen(main_buffer) - 1);
    break;
  }
}

// void print_output(void) {
//   if (strlen(vars_buffer) > 0) {
//     printf(";---vars---\n%s\n", vars_buffer);
//   }
//   if (strlen(temp_buffer) > 0) {
//     printf(";---temp---\n%s\n", temp_buffer);
//   }
//
//   printf("org 00h\n");
//   printf("main:\n");
//   printf("%s", main_buffer);
//   printf("hlt\n");
// }

void print_output() {
  if (file_out == NULL) {
    file_out = stdout;
  }

  fprintf(file_out, "\norg 80h\n");
  if (strlen(vars_buffer) > 0) {
    fprintf(file_out, ";---vars---\n%s\n", vars_buffer);
  }
  if (strlen(temp_buffer) > 0) {
    fprintf(
        file_out,
        ";---temp comparacoes---\n_tc1: db 0\n_tc2: db 0\n\n;---temp---\n%s\n",
        temp_buffer);
  }

  fprintf(file_out, "org 00h\n");
  fprintf(file_out, "main:\n");
  fprintf(file_out, "%s", main_buffer);
  fprintf(file_out, "hlt\n");
}
