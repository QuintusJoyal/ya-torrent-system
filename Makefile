# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Iinclude -g
LDFLAGS = -lssl -lcrypto

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = bin
TESTDIR = tests
TESTBUILDDIR = $(TESTDIR)/$(BUILDDIR)
TESTBINDIR = $(TESTDIR)/$(BINDIR)

# Create output directories if they don't exist
$(shell mkdir -p $(BUILDDIR) $(BINDIR) $(TESTBUILDDIR) $(TESTBINDIR))

# Executables
# CLIENT_EXEC = $(BINDIR)/client
CLIENT_EXEC = $(BINDIR)/cli2219
# SERVER_EXEC = $(BINDIR)/server
SERVER_EXEC = $(BINDIR)/srv6088
TEST_CLIENT_EXEC = $(TESTBINDIR)/test_client

# Source files
CLIENT_SRC = $(SRCDIR)/client.c
CLI2219_SRC = $(SRCDIR)/cli2219.c
SERVER_SRC = $(SRCDIR)/server.c
SRV6088_SRC = $(SRCDIR)/srv6088.c
LOGGER_SRC = $(SRCDIR)/logger.c
PROTOCOL_SRC = $(SRCDIR)/protocol.c

# Test source files
TEST_CLIENT_SRC = $(TESTDIR)/test_client.c

# Object files
CLIENT_OBJ = $(BUILDDIR)/client.o
CLI2219_OBJ = $(BUILDDIR)/cli2219.o
SERVER_OBJ = $(BUILDDIR)/server.o
SRV6088_OBJ = $(BUILDDIR)/srv6088.o
LOGGER_OBJ = $(BUILDDIR)/logger.o
PROTOCOL_OBJ = $(BUILDDIR)/protocol.o

# Test object files
TEST_CLIENT_OBJ = $(TESTBUILDDIR)/test_client.o

# Build all (default target)
all: $(CLIENT_EXEC) $(SERVER_EXEC) $(TEST_CLIENT_EXEC)

# Compile protocol object
$(PROTOCOL_OBJ): $(PROTOCOL_SRC) $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile logger object
$(LOGGER_OBJ): $(LOGGER_SRC) $(INCDIR)/logger.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile client object
$(CLIENT_OBJ): $(CLIENT_SRC) $(INCDIR)/client.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@
$(CLI2219_OBJ): $(CLI2219_SRC) $(INCDIR)/client.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile server object
$(SERVER_OBJ): $(SERVER_SRC) $(INCDIR)/server.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@
$(SRV6088_OBJ): $(SRV6088_SRC) $(INCDIR)/server.h $(INCDIR)/logger.h $(INCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test client object
$(TEST_CLIENT_OBJ): $(TEST_CLIENT_SRC) $(INCDIR)/client.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link client executable
$(CLIENT_EXEC): $(CLI2219_OBJ) $(CLIENT_OBJ) $(LOGGER_OBJ) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Link server executable
$(SERVER_EXEC): $(SRV6088_OBJ) $(SERVER_OBJ) $(LOGGER_OBJ) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Link test client executable
$(TEST_CLIENT_EXEC): $(TEST_CLIENT_OBJ) $(CLIENT_OBJ) $(LOGGER_OBJ) $(PROTOCOL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Clean build files
clean:
	rm -rf $(BUILDDIR) $(BINDIR) $(TESTBUILDDIR) $(TESTBINDIR)

# Phony targets
.PHONY: all clean
