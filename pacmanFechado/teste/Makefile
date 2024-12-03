all:
	as -o LIB.o Biblioteca_GPU.s
	gcc -std=c99 main.c -lintelfpgaup LIB.o -o main -lpthread
	sudo ./main