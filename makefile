all:
	gcc -std=gnu99 -O3 -o httpd main.c server.c -levent

debug:
	gcc -std=gnu99 -ggdb -Og -Wextra -Werror -o httpd main.c server.c -levent

clean:
	rm httpd

