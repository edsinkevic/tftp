CC = gcc
OBJ = $(CURDIR)/obj
DEPS = $(CURDIR)/src/deps
SRC = $(CURDIR)/src
BIN = $(CURDIR)/bin
CLIENTDEPS = $(OBJ)/client.o $(OBJ)/edutils.o

client: $(CLIENTDEPS)
	$(CC) -Wall $(CLIENTDEPS) -o $(BIN)/$@

$(OBJ)/%.o: $(SRC)/%/main.c $(OBJ)
	$(CC) -o $@ -c $<

$(OBJ)/%.o: $(DEPS)/%.c $(DEPS)/%.h $(OBJ)
	$(CC) -o $@ -c $<

$(OBJ):
	mkdir $(OBJ) $(BIN)

clean:
	rm -rf obj bin

build: client