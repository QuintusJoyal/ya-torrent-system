#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "protocol.h"

// Function prototypes
int connect_to_server(const char *server_ip, int port);
void request_file_list(int sock);
void download_file(int sock, const char *filename);
void upload_file(int sock, const char *filename);
void display_progress(long total_size, long downloaded);
long get_file_size(int sock, const char *filename);
int calculate_file_hash(const char *file_path, long offset, char *hash_output);
int request_file_metadata(int sock, const char *filename, long offset, Payload *metadata);

#endif /* CLIENT_H */
