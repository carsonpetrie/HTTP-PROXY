CFLAGS = -Wall -Wextra -Wpedantic -Wshadow

httpproxy:	httpproxy.o queue.o cache.o
		cc -pthread -o httpproxy httpproxy.o queue.o cache.o

httpproxy.o:	httpproxy.h httpproxy.c
		cc -pthread -c httpproxy.c

queue.o:	queue.h queue.c
		cc -pthread -c queue.c

cache.o:	cache.h cache.c
		cc -pthread -c cache.c

clean:
		rm -f httpproxy httpproxy.o queue.o cache.o	
