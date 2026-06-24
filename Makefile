CC = clang
CFLAGS = -Wall -Wextra -std=c17 -O2 -D_DEFAULT_SOURCE -D_GNU_SOURCE
LIBS = -lz
TARGET = undo
SRC = src/main.c src/utils.c src/config.c src/storage.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
