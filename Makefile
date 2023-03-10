.RECIPEPREFIX = >
CC=gcc -g
CFLAGS= -Wall -lpthread -lssl -lcrypto -Wextra -Wpedantic

all: myproxy

myproxy: ./src/proxy.o
> $(CC) ./src/proxy.o -o ./bin/myproxy -lpthread -lssl -lcrypto

./src/proxy.o: ./src/proxyMain.c
> $(CC) $(CFLAGS) -c ./src/proxyMain.c -o ./src/proxy.o

clean:
> rm -rf ./src/*.o
> rm -rf ./bin/myproxy
