#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <arpa/inet.h>

/// Operation codes for communication
#define OP_DOWNLOAD       1  ///< Download operation
#define OP_UPLOAD         2  ///< Upload operation
#define OP_LIST_FILES     3  ///< Request file list
#define OP_REQ_META_DATA  4  ///< Request file metadata
#define OP_META_DATA      5  ///< Metadata operation
#define OP_EXIT           6  ///< Exit operation

/// Status codes for file operations
#define STAT_FILE_FOUND           100 ///< File found
#define STAT_FILE_NOT_FOUND       101 ///< File not found
#define STAT_FILE_UNCHANGED       102 ///< File has not changed
#define STAT_FILE_CHANGED         103 ///< File has changed
#define STAT_FILE_VERIFY          104 ///< File verification status
#define STAT_SERVER_ERROR         105 ///< Server error

/// Constants for file handling
#define MAX_FILENAME 256      ///< Maximum length of filename
#define CHUNK_SIZE 1024       ///< Size of data chunks for transfer
#define HASH_SIZE (SHA256_DIGEST_LENGTH * 2 + 1) ///< Size of hash string

/// Structure for communication payload
typedef struct {
    int operation;        ///< Operation type (e.g., OP_DOWNLOAD, OP_UPLOAD)
    int status;           ///< Status of the response
    int filename_length;  ///< Length of the filename (if applicable)
    long offset;          ///< Offset for resuming downloads (if applicable)
    char filename[MAX_FILENAME];  ///< The filename (if applicable)
    long file_size;       ///< Size of the file (for upload/download/metadata operations)
    char hash[HASH_SIZE]; ///< File hash for integrity checks (optional)
} Payload;

/**
 * @brief Send a payload over the socket.
 *
 * @param sock The socket descriptor.
 * @param payload Pointer to the payload to send.
 * @return 0 on success, -1 on failure.
 */
int send_payload(int sock, const Payload *payload);

/**
 * @brief Receive a payload from the socket.
 *
 * @param sock The socket descriptor.
 * @param payload Pointer to the payload to receive.
 * @return 0 on success, -1 on failure.
 */
int receive_payload(int sock, Payload *payload);

/**
 * @brief Calculate the SHA-256 hash of a specific chunk of a file.
 *
 * @param file_path Path to the file.
 * @param offset The offset from which to calculate the hash.
 * @param hash_output Buffer to store the resulting hash string.
 * @return 0 on success, -1 on failure.
 */
int calculate_file_hash(const char *file_path, long offset, char *hash_output);

#endif // PROTOCOL_H
