# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread

# Executable name
EXEC = rejs

# Source files
SRCS = rejs.c utils.c passenger.c shipCaptain.c harbourCaptain.c

# Header files
HEADERS = utils.h passenger.h

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(EXEC)

# Link the executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(EXEC) $(OBJS)

# Run the program
run: $(EXEC)
	./$(EXEC)
