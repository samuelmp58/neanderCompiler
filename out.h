#ifndef OUT_H
#define OUT_H

#include "compiler.h"

typedef enum { SECTION_VARS, SECTION_TEMP, SECTION_MAIN } SectionType;

void init_output(void);
void emit_fmt(SectionType section, const char *format, ...);
void print_output(void);

#endif // OUT_H
