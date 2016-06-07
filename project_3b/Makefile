dist_files = Makefile README lab3b.py

default: clean lab3b

run: clean lab3b

lab3b: lab3b.py
	python lab3b.py

clean:
	rm -f lab3b lab3b_check.txt comp.txt output.txt diff.txt *~ \#*
dist: $(dist-files)
	tar -cvzf lab3b-404403562.tar.gz $(dist_files)

check:
	python lab3b.py >output.txt ; \
	sort output.txt >comp.txt ; \
	diff comp.txt check.txt >diff.txt ;

.PHONY: clean dist check default
