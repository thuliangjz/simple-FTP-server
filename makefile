server: server.c utils.c processors.c workers.c 
	gcc -Wall server.c utils.c processors.c workers.c -o server -lpthread
clean:
	rm server
