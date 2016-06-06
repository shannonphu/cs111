default: lab0

lab0: lab0.c
	gcc -g lab0.c -o lab0
clean:
	find . -type f -not -name 'Makefile' -not -name 'lab0.c' -not -name 'README' -not -name '*.png' -not -name '*.tar.gz' -delete
dist:
	tar -cvzf lab0-404403562.tar.gz lab0.c Makefile README gdb1.png gdb2.png
check: lab0
	@printf 'Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce mollis eros ac dignissim cursus. Donec at ornare sem. Aenean ac ante et lacus vestibulum bibendum. Mauris mattis lectus quis purus mattis finibus. Curabitur porta rhoncus turpis, facilisis pharetra neque rutrum eget. Praesent in gravida sem. Nunc vel imperdiet magna. Nullam condimentum egestas gravida. Morbi elementum pulvinar ligula ornare dictum. Donec et magna sit amet lorem hendrerit sagittis. Ut nec ultricies nibh, sed congue felis. Sed ullamcorper blandit nunc sit amet ullamcorper. Proin imperdiet ultrices felis non placerat.' > input.txt

	./lab0 --input=input.txt | diff input.txt -
	@echo "Passed --input arg test."
	@BAD_INPUT=$$(./lab0 --input=nonexistent >/dev/null 2>&1; echo $$?); \
	if [ "$$BAD_INPUT" -ne 1 ]; then \
		echo "Did not pass bad input test."; \
	else \
		echo "Passed bad input file test."; \
	fi;
	./lab0 --input=input.txt --output=output.txt
	diff input.txt output.txt
	@echo "Passed --input --output test."
	@echo "./lab0 --segfault --catch"
	@SEG_RES=$$(./lab0 --segfault --catch >/dev/null 2>&1; echo $$?); \
	if [ "$$SEG_RES" -ne 3 ]; then \
		echo "Did not pass --segfault --catch test."; \
	else \
		echo "Passed --segfault --catch test."; \
	fi;
	@echo "./lab0 --segfault"
	@SEG_RES=$$(./lab0 --segfault; echo $$?); \
	if [ "$$SEG_RES" -ne 139 ]; then \
		echo "Did not pass --segfault test."; \
	else \
		echo "Passed --segfault test."; \
	fi; \
	rm input.txt output.txt
