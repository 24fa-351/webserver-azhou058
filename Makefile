webserver: webserver.c
	gcc -o webserver webserver.c

clean:
	rm webserver