#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "protocol.h"

#define MAX_CLIENTS 10

// Shared directory for files
extern char SHARED_DIR[MAX_FILENAME];

// Function prototypes
void set_socket_options(int sock);
void handle_client(int client_sock, struct sockaddr_in client_addr);
void send_file_list(int client_sock);
void send_file_metadata(int client_sock, const char *filename, long offset);
void send_request_metadata(int client_sock, const char *filename);
void send_file(int client_sock, const char *filename, long offset);
void receive_file(int client_sock, const char *filename, long file_size);
int calculate_file_hash(const char *file_path, long offset, char *hash_output);

#endif /* SERVER_H */