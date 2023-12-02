.PHONY: all, clean

all:
	gcc -o chat main.c -lpthread
clean:
	rm -rf chat