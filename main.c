#include "compiler.h"
#include "out.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ----- Suportes -----
 *
 * if, else if, else
 * 
 * while
 *
 * < > <= >= == !=
 *
 * print
 *
 * declarações com ','
 *
 */

const char *compiler_name = "NC";

int main(int argc, char *argv[]) {
  char *program_name = "a";
  char *author_name = "";
  char *input_filename = NULL;
  int opt;

  while ((opt = getopt(argc, argv, "o:a:")) != -1) {
    switch (opt) {
    case 'o':
      program_name = optarg;
      break;
    case 'a':
      author_name = optarg;
      break;
    case '?':
      fprintf(stderr, "Uso: %s [-o nome_saida] [-a autor] <arquivo_fonte>\n",
              argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    input_filename = argv[optind];
  } else {
    fprintf(stderr, "Erro: Nenhum arquivo de entrada fornecido.\n");
    fprintf(stderr, "Uso: %s [-o nome_saida] [-a autor] <arquivo_fonte>\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  char output_filename[256];
  snprintf(output_filename, sizeof(output_filename), "%s.asm", program_name);

  file_out = fopen(output_filename, "w");
  if (file_out == NULL) {
    perror("Erro ao criar arquivo de saída");
    return EXIT_FAILURE;
  }

  // emitir("LOAD t0", SECAO_MAIN);
  // fputs("Primeira linha do arquivo\n"
  //       "Segunda linha com mais informações\n",
  //       file_out);
  //
  // fprintf(file_out,
  // ";---------------------------------------------------\n");
  // fprintf(file_out, "; Programa: %s\n", program_name);
  // fprintf(file_out, "; Autor: %s \n", author_name);
  // fprintf(file_out, ";*Compilado por %s* \n", compiler_name);
  // fprintf(file_out,
  // ";---------------------------------------------------\n");

  char data_formatada[12];
  time_t tempo_atual = time(NULL);
  struct tm *info_tempo = localtime(&tempo_atual);
  strftime(data_formatada, sizeof(data_formatada), "%d/%m/%Y", info_tempo);

  fprintf(file_out, ";---------------------------------------------------\n");
  fprintf(file_out, ";─────▄───▄      Nome: %s\n", program_name);
  fprintf(file_out, ";─▄█▄─█▀█▀█─▄█▄  Autor: %s\n", author_name);
  fprintf(file_out, ";▀▀████▄█▄████▀▀ Data: %s\n", data_formatada);
  fprintf(file_out, ";─────▀█▀█▀      *Compilado usando %s* \n", compiler_name);
  fprintf(file_out, ";---------------------------------------------------\n");

  FILE *file = fopen(input_filename, "r");
  if (file == NULL) {
    fprintf(stderr, "Erro ao abrir o arquivo de entrada '%s'\n",
            input_filename);
    fclose(file_out);
    return EXIT_FAILURE;
  }

  compile(file);

  fclose(file);
  print_output();
  fclose(file_out);

  return EXIT_SUCCESS;
}
