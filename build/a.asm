;---------------------------------------------------
;─────▄───▄      Nome: a
;─▄█▄─█▀█▀█─▄█▄  Autor: 
;▀▀████▄█▄████▀▀ Data: 29/08/2026
;─────▀█▀█▀      *Compilado usando NC* 
;---------------------------------------------------

org 80h
;---vars---
x: db 2
y: db 1

;---temp comparacoes---
_tc1: db 0
_tc2: db 0

;---temp---
_t0: db 0
_t1: db 0
_t2: db 0

org 00h
main:
	lda y
	sta _t0
	ldi 1
	sta _t1
	lda _t0
	add _t1
	sta _t2
	lda _t2
	sta y
	lda x
	sta _tc1

	lda y
	sta _tc2


	; --- Teste da Condicao (x == y) do IF #0 ---

	lda _tc1
	sub _tc2
	jnz _elseif_0_0

	; --- Inicio do Corpo do IF #0 ---

	lda y
	sta _t0
	ldi 3
	sta _t1
	lda _t0
	add _t1
	sta _t2
	lda _t2
	sta y

_while_start_0:
	; --- Teste da Condicao do WHILE #0 ---

	lda y
	sta _tc1

	lda x
	sta _tc2

	lda _tc2
	sub _tc1
	jn _skip_false_0
	jmp _while_end_0

_skip_false_0:

	; --- Inicio do Corpo do WHILE #0 ---

	lda y
	out 0

	lda y
	sta _t0
	ldi 1
	sta _t1
	lda _t1
	not
	sta _t2
	ldi 1
	add _t2
	add _t0
	sta _t2
	lda _t2
	sta y
	jmp _while_start_0

_while_end_0:
	; --- Fim do WHILE #0 ---

	jmp _if_end_0


_elseif_0_0:


_if_end_0:
	; --- Fim da Estrutura IF #0 ---

hlt
