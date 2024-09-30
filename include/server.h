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

#define MAX_CLIENTS 10 ///< Maximum number of simultaneous clients

/// Shared directory for files
extern char SRC_DIR[MAX_FILENAME]; 

/**
 * @brief Set options for the socket to optimize performance and reliability.
 *
 * @param sock The socket descriptor.
 */
void set_socket_options(int sock);

/**
 * @brief Handle communication with a connected client.
 *
 * @param client_sock The socket descriptor for the connected client.
 * @param client_addr The address of the connected client.
 */
void handle_client(int client_sock, struct sockaddr_in client_addr);

/**
 * @brief Send the list of available files in the shared directory to the client.
 *
 * @param client_sock The socket descriptor for the connected client.
 */
void send_file_list(int client_sock);

/**
 * @brief Send metadata for a specific file to the client.
 *
 * @param client_sock The socket descriptor for the connected client.
 * @param filename The name of the file whose metadata is to be sent.
 * @param offset The offset from which the file should be sent (for resuming).
 */
void send_file_metadata(int client_sock, const char *filename, long offset);

/**
 * @brief Request and send the metadata for a specific file.
 *
 * @param client_sock The socket descriptor for the connected client.
 * @param filename The name of the file whose metadata is requested.
 */
void send_request_metadata(int client_sock, const char *filename);

/**
 * @brief Send a file in chunks to the client.
 *
 * @param client_sock The socket descriptor for the connected client.
 * @param filename The name of the file to send.
 * @param offset The offset from which to start sending the file.
 */
void send_file(int client_sock, const char *filename, long offset);

/**
 * @brief Receive a file from the client and save it to the shared directory.
 *
 * @param client_sock The socket descriptor for the connected client.
 * @param filename The name of the file to receive.
 * @param file_size The expected size of the file.
 */
void receive_file(int client_sock, const char *filename, long file_size);

/**
 * @brief Calculate the SHA-256 hash of a specific chunk of a file.
 *
 * @param file_path Path to the file.
 * @param offset The offset from which to calculate the hash.
 * @param hash_output Buffer to store the resulting hash string.
 * @return 0 on success, -1 on failure.
 */
int calculate_file_hash(const char *file_path, long offset, char *hash_output);

#endif /* SERVER_H */
