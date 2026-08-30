# Neander Compiler (`nc`)
Um compilador simples para **Neander / Neanderlin**. Transpila o código de alto nível para linguagem assembly do Neander `.asm`.

## Uso
./nc <arquivo.n> [-o <saida.asm>] [-a <autor>]

---

## Funcionalidades
- Suporte a variáveis declaradas e inicializadas (`var x = 2;`).
- Expressões matemáticas.
- if, elseif, else
- while
- print

### Observações
- Toda manipulação de valor em variável se deve usar **var** antes.
- Sem suporte a && e ||.
