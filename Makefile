CC=gcc
FLAGS=-g -Wall
SRC=$(wildcard src/*.c)
SERVER=$(filter-out src/verokv-cli.c, $(SRC))
CLIENT=$(filter-out src/verokv.c, $(SRC))
TEST=$(filter-out src/verokv.c src/verokv-cli.c, $(wildcard $(SRC) tests/*.c))

all: server client

server: $(SERVER)
	$(CC) $(FLAGS) $(SERVER) -o verokv -lm
client: $(CLIENT)
	$(CC) $(FLAGS) $(CLIENT) -o verokv-cli -lm
test: $(TEST)
	$(CC) $(FLAGS) $(TEST) -o test.out -lm && ./test.out
clean:
	rm verokv* *.out
