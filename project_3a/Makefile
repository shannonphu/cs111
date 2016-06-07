default: lab3a

lab3a: lab3a.c
	gcc -g lab3a.c -o lab3a

clean:
	rm -f lab3a *~ \#* *.csv
dist:
	tar -cvzf lab3a-404403562.tar.gz lab3a.c Makefile README

check: lab3a
	./lab3a disk-image
	diff super.csv soln/super.csv
	diff group.csv soln/group.csv
	diff bitmap.csv soln/bitmap.csv
	diff inode.csv soln/inode.csv
	diff directory.csv soln/directory.csv
	diff indirect.csv soln/indirect.csv
