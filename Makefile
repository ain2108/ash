CC=gcc
CFLAGS= -I -Wall

w4118_sh: shell.o 
	$(CC) -o w4118_sh shell.o $(CFLAGS)

clean:
	rm shell.o w4118_sh 