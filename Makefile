EXEC = httpserver
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=%.o)

CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -g -pthread -O3
LDFLAGS = 
PROGRAM = httpserver
FILES = httpserver.o request.o queue.o

all: $(PROGRAM)

$(PROGRAM): $(FILES)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(FILES) $(LDFLAGS)

clean: 
	rm -f $(PROGRAM) $(FILES)

format: 
	clang-format -i -style=file *.[ch]

scan-build: clean
	scan-build make     
