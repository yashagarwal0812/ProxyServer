# Compiler and flags
CC = g++
CFLAGS = -g -Wall -std=c++17

# Targets
all: proxy

# Build object files and the final executable
proxy: with_cache.cpp proxy_parse.c
	$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c -lpthread
	$(CC) $(CFLAGS) -o proxy.o -c with_cache.cpp -lpthread
	$(CC) $(CFLAGS) -o proxy proxy_parse.o proxy.o -lpthread

# Clean up generated files
clean:
	rm -f proxy *.o

# Create tar archive
tar:
	tar -cvzf ass1.tgz with_cache.cpp README Makefile proxy_parse.c proxy_parse.h
