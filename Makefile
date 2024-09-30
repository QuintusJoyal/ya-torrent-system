# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Iinclude -g
LDFLAGS = -lssl -lcrypto

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = bin

# Create output directories if they don't exist
$(shell mkdir -p $(BUILDDIR) $(BINDIR))

# Executables
CLIENT_EXEC = $(BINDIR)/client
SERVER_EXEC = $(BINDIR)/server

# Source files
CLIENT_SRC = $(SRCDIR)/client.c
SERVER_SRC = $(SRCDIR)/server.c
LOGGER_SRC = $(SRCDIR)/logger.c
PROTOCOL_SRC = $(SRCDIR)/protocol.c

# Object files
CLIENT_OBJ = $(BUILDDIR)/client.o
SERVER_OBJ = $(BUILDDIR)/server.o
LOGGER_OBJ = $(BUILDDIR)/logger.o
PROTOCOL_OBJ = $(BUILDDIR)/protocol.o

# Build all (default target)
all: $(CLIENT_EXEC) $(SERVER_EXEC)

# Compile protocol object
$(PROTOCOL_OBJ): $(PROTOCOL_SRC) $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile logger object
$(LOGGER_OBJ): $(LOGGER_SRC) $(INCDIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile client object
$(CLIENT_OBJ): $(CLIENT_SRC) $(INCDIR)/client.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile server object
$(SERVER_OBJ): $(SERVER_SRC) $(INCDIR)/server.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link client executable
$(CLIENT_EXEC): $(CLIENT_OBJ) $(LOGGER_OBJ) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Link server executable
$(SERVER_EXEC): $(SERVER_OBJ) $(LOGGER_OBJ) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Clean build files
clean:
	rm -rf $(BUILDDIR)/*.o $(BINDIR)/*

# Phony targets
.PHONY: all clean
