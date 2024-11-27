all:
	as -o LIB.o Biblioteca_GPU.s
	gcc -std=c99 teste.c -lintelfpgaup LIB.o -o teste
	sudo ./teste