#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <arpa/inet.h>

#define OP_DOWNLOAD       1
#define OP_UPLOAD         2
#define OP_LIST_FILES     3
#define OP_REQ_META_DATA  4
#define OP_META_DATA      5
#define OP_EXIT           6

#define STAT_FILE_FOUND           100
#define STAT_FILE_NOT_FOUND       101
#define STAT_FILE_UNCHANGED       102
#define STAT_FILE_CHANGED         103
#define STAT_FILE_VERIFY          104
#define STAT_SERVER_ERROR         105

#define MAX_FILENAME 256
#define CHUNK_SIZE 1024
#define HASH_SIZE SHA256_DIGEST_LENGTH * 2 + 1

// Define a struct to represent the communication payload
typedef struct {
    int operation;        // Operation type: OP_DOWNLOAD, OP_UPLOAD, OP_LIST_FILES, etc.
    int status;           // Status of the response
    int filename_length;  // Length of the filename (if applicable)
    long offset;          // Offset for resuming downloads (if applicable)
    char filename[MAX_FILENAME];  // The filename (if applicable)
    long file_size;       // File size (for upload/download/metadata operations)
    char hash[HASH_SIZE]; // File hash for integrity check (optional, for relevant operations)
} Payload;

int send_payload(int sock, Payload *payload);
int receive_payload(int sock, Payload *payload);
int calculate_file_hash(const char *file_path, long offset, char *hash_output);

#endif // PROTOCOL_H
