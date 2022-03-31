compile:
	clang-12 -std=c99 -Wall -pedantic *.c -L. -lruntime

run:
	./a.out -p 5 10 20 30 40 50
