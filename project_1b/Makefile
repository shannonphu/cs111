default: client server

client: client.c
	gcc -g -w -lpthread -lmcrypt client.c -o client

server: server.c
	gcc -g -w -lpthread -lmcrypt server.c -o server

clean:
	rm -f client server *~
dist:
	tar -cvzf lab1b-404403562.tar.gz client.c server.c Makefile README my.key
