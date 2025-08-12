CC=g++
CFLAGS= -g -Wall 

all: cache_proxy

cache_proxy: proxy_server.c
	$(CC) $(CFLAGS) -o http_parser.o -c http_parser.c -lpthread
	$(CC) $(CFLAGS) -o cache_proxy.o -c proxy_server.c -lpthread
	$(CC) $(CFLAGS) -o cache_proxy http_parser.o cache_proxy.o -lpthread

clean:
	rm -f cache_proxy *.o

tar:
	tar -cvzf ass1.tgz proxy_server.c README Makefile http_parser.c http_parser.h