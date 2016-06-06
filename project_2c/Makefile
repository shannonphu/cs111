default: lab2c

lab2c: lab2c.c SortedList.h SortedList.c
	gcc -g -lpthread lab2c.c SortedList.c -o lab2c

clean: 
	rm -f lab2c *~
dist:
	tar -cvzf lab2c-404403562.tar.gz lab2c.c SortedList.[ch] Makefile README *.png
