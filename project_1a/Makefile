default: lab1a

lab1a: lab1a.c
	gcc -g -lpthread lab1a.c -o lab1a
clean:
	rm -f lab1a *~ \#*
dist:
	tar -cvzf lab1a-404403562.tar.gz lab1a.c Makefile README
