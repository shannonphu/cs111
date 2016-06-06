default: lab2a

lab2a: lab2a.c
	gcc -g -lpthread lab2a.c -o lab2a

clean: lab2a
	rm -f lab2a *~
dist:
	tar -cvzf lab2a-404403562.tar.gz lab2a.c Makefile README *.png
