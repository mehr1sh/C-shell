# Define the C compiler and flags
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm -Iinclude

# Define the source and object files
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, src/%.o, $(SRCS))

# Define the executable name
EXEC = shell.out

# Default rule to build the executable
all: $(EXEC)

# Rule to link all object files into the executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

# Rule to compile each source file into an object file
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove generated files
clean:
	rm -f $(OBJS) $(EXEC) .shell_history