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

// Define maximum filename length
#define MAX_FILENAME 256

// Shared directory for files
extern char DEST_DIR[MAX_FILENAME];

// Function prototypes

/**
 * @brief Connect to the server at the specified IP and port.
 *
 * @param server_ip The IP address of the server.
 * @param port The port number of the server.
 * @return A socket descriptor on success, -1 on failure.
 */
int connect_to_server(const char *server_ip, int port);

/**
 * @brief Request metadata for a specific file from the server.
 *
 * @param sock The socket descriptor for communication with the server.
 * @param filename The name of the file for which metadata is requested.
 * @param offset The current offset in the file.
 * @param metadata A pointer to a Payload structure to receive the metadata.
 * @return 0 on success, -1 on failure.
 */
int request_file_metadata(int sock, const char *filename, long offset, Payload *metadata);

/**
 * @brief Request the list of available files from the server.
 *
 * @param sock The socket descriptor for communication with the server.
 */
void request_file_list(int sock);

/**
 * @brief Download a file from the server.
 *
 * @param sock The socket descriptor for communication with the server.
 * @param filename The name of the file to download.
 */
void download_file(int sock, const char *filename);

/**
 * @brief Upload a file to the server.
 *
 * @param sock The socket descriptor for communication with the server.
 * @param filename The name of the file to upload.
 */
void upload_file(int sock, const char *filename);

/**
 * @brief Display download progress to the user.
 *
 * @param total_size The total size of the file being downloaded.
 * @param downloaded The amount of the file that has been downloaded so far.
 */
void display_progress(long total_size, long downloaded);

/**
 * @brief Send an exit request to the server to close the connection.
 *
 * @param sock The socket descriptor for communication with the server.
 */
void send_exit_request(int sock);

#endif /* CLIENT_H */
