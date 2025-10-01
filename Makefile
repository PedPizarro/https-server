# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpthread  # Remove if not using threads

# Target binary and source files
TARGET = server
SRC = https-server.c

# Default build target: remove old binary and compile
all:
	@rm -f $(TARGET)
	@echo "Compiling $(TARGET)..."
	@$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "Build complete."

# Run target: build and start the server
run:
	@rm -f $(TARGET)
	@echo "Compiling $(TARGET)..."
	@$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "Starting server..."
	@./$(TARGET)

# Clean target: remove binary only
clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET)

.PHONY: all run clean
