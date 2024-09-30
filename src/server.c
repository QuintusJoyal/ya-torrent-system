#include "server.h"
#include "logger.h"
#include "protocol.h"
#include <sys/stat.h>

char SRC_DIR[MAX_FILENAME];

// Function to set socket options
void set_socket_options(int sock) {
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error setting SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("Error setting SO_REUSEPORT");
        exit(EXIT_FAILURE);
    }
#endif
}

// Function to handle client connections
void handle_client(int client_sock, struct sockaddr_in client_addr) {
    Payload payload;

    while (1) {  // Infinite loop to continuously handle requests
        // Receive the payload from the client
        if (receive_payload(client_sock, &payload) != 0) {
            log_message(LOG_ERROR, "Error receiving payload from client");
            break;  // Exit loop on error
        }

        // Handle operations based on the payload type
        switch (payload.operation) {
            case OP_DOWNLOAD:
                // Proceed with download if the metadata is accepted
                send_file(client_sock, payload.filename, payload.offset);
                break;

            case OP_UPLOAD:
                // Server requests metadata from the client
                send_request_metadata(client_sock, payload.filename);
                break;

            case OP_REQ_META_DATA:
                // Client requested metadata about a file, including offset
                log_message(LOG_INFO, "Client requested metadata for %s at offset %ld", payload.filename, payload.offset);
                send_file_metadata(client_sock, payload.filename, payload.offset);
                break;

            case OP_META_DATA:
                // Server receives metadata from the client before uploading the file
                log_message(LOG_INFO, "Received file metadata from client: %s, size: %ld", payload.filename, payload.file_size);
                receive_file(client_sock, payload.filename, payload.file_size);
                break;

            case OP_LIST_FILES:
                // Send the list of files
                send_file_list(client_sock);
                break;

            case OP_EXIT:
                printf("Client requested to close the connection.\n");
                log_message(LOG_INFO, "Client requested to close the connection.");
                return;  // Exit function directly

            default:
                printf("Invalid operation received from client\n");
                log_message(LOG_ERROR, "Invalid operation received from client");
                break;
        }
    }

    // Clean up and close the client socket
    close(client_sock);
    log_message(LOG_INFO, "Client connection closed.");
}

// Function to send metadata about a file to the client
void send_file_metadata(int client_sock, const char *filename, long offset) {
    Payload metadata_payload;
    struct stat file_stat;

    char file_path[MAX_FILENAME];
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", SRC_DIR, filename);

    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    if (stat(file_path, &file_stat) == 0) {
        long file_size = file_stat.st_size;
        char hash[HASH_SIZE];

        // If the offset is greater than zero, calculate the hash of the last chunk
        if (offset > 0) {
            if (calculate_file_hash(file_path, offset, hash) == 0) {
                // Initialize the metadata payload
                memset(&metadata_payload, 0, sizeof(metadata_payload));

                metadata_payload.operation = OP_META_DATA;
                memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename));
                metadata_payload.file_size = file_size;
                memcpy(metadata_payload.hash, hash, sizeof(metadata_payload.hash));
                metadata_payload.status = STAT_FILE_VERIFY;

            } else {
                log_message(LOG_ERROR, "Error calculating hash for file '%s'", file_path);
                return;
            }
        } else {
            // No offset means this is a new upload; send an empty hash
            memset(&metadata_payload, 0, sizeof(metadata_payload));

            metadata_payload.operation = OP_META_DATA;
            memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename));
            metadata_payload.file_size = file_size;
            metadata_payload.status = STAT_FILE_FOUND;
        }

        send_payload(client_sock, &metadata_payload);
        log_message(LOG_INFO, "Sent metadata for file: %s with hash", filename);
    } else {
        log_message(LOG_ERROR, "Error retrieving file metadata for %s", filename);
    }
}

// Function to send request for metadata
void send_request_metadata(int client_sock, const char *filename) {
    Payload req_payload;
    memset(&req_payload, 0, sizeof(req_payload));

    req_payload.operation = OP_REQ_META_DATA;
    memcpy(req_payload.filename, filename, sizeof(req_payload.filename));

    send_payload(client_sock, &req_payload);
    log_message(LOG_INFO, "Requested metadata for file: %s", filename);
}

// Function to send the list of available files in the shared directory
void send_file_list(int client_sock) {
    DIR *dir;
    struct dirent *entry;
    char file_list[1024] = "";  // Buffer to hold file list
    size_t current_length = 0;   // Track the current length of the file_list

    dir = opendir(SRC_DIR);
    if (dir == NULL) {
        perror("Error opening shared directory");
        log_message(LOG_ERROR, "Error opening shared directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Regular file
            // Check if adding this file name would exceed the buffer size
            if (current_length + strlen(entry->d_name) + 1 < sizeof(file_list)) {
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
                current_length += strlen(entry->d_name) + 1; // +1 for the newline
            } else {
                log_message(LOG_INFO, "File list too long, truncating");
                break;  // Stop adding more files to avoid overflow
            }
        }
    }

    closedir(dir);

    // Check if the file list is empty and send appropriate message
    if (strlen(file_list) == 0) {
        const char *no_files_message = "No files available in the shared directory.\n";
        send(client_sock, no_files_message, strlen(no_files_message), 0);
    } else {
        // Send the file list directly
        if (send(client_sock, file_list, strlen(file_list), 0) < 0) {
            perror("Error sending file list");
            log_message(LOG_ERROR, "Error sending file list to client");
        }
    }
}

// Function to send a file in chunks from a specific offset
void send_file(int client_sock, const char *filename, long offset) {
    char file_path[MAX_FILENAME];
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", SRC_DIR, filename);

    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Open the file for reading
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        log_message(LOG_ERROR, "Error opening file: %s", file_path);
        return;
    }

    // Move the file pointer to the desired offset
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Error seeking to file offset");
        log_message(LOG_ERROR, "Error seeking to offset %ld in file: %s", offset, file_path);
        fclose(file);
        return;
    }

    char buffer[CHUNK_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        int bytes_sent = 0;
        // Ensure the entire buffer is sent
        while (bytes_sent < bytes_read) {
            int send_result = send(client_sock, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (send_result < 0) {
                perror("Error sending file");
                log_message(LOG_ERROR, "Error sending file: %s", file_path);
                fclose(file);
                return;
            }
            bytes_sent += send_result;
        }
    }

    if (ferror(file)) {
        perror("Error reading file");
        log_message(LOG_ERROR, "Error reading file: %s", file_path);
    }

    fclose(file);
    log_message(LOG_INFO, "Successfully sent file: %s from offset: %ld", file_path, offset);
}

// Function to receive a file from the client and save it (with overwrite and reliability)
void receive_file(int client_sock, const char *filename, long expected_file_size) {
    char file_path[MAX_FILENAME];
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", SRC_DIR, filename);

    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Open the file in "wb" mode to overwrite if it exists
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        perror("Error creating file");
        log_message(LOG_ERROR, "Error creating file: %s", file_path);
        return;
    }

    char buffer[CHUNK_SIZE];
    int bytes_received;
    long total_bytes_received = 0;

    // Loop to receive chunks until the full file is received
    while (total_bytes_received < expected_file_size) {
        // Calculate the remaining bytes to receive in this iteration
        long bytes_remaining = expected_file_size - total_bytes_received;
        int chunk_size = (bytes_remaining < CHUNK_SIZE) ? bytes_remaining : CHUNK_SIZE;

        // Receive the next chunk
        bytes_received = recv(client_sock, buffer, chunk_size, 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                log_message(LOG_INFO, "Connection closed by client before full file was received");
            } else {
                perror("Error receiving file");
                log_message(LOG_ERROR, "Error receiving file: %s", file_path);
            }
            fclose(file);
            return;
        }

        // Write the received chunk to the file
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            perror("Error writing to file");
            log_message(LOG_ERROR, "Error writing to file: %s", file_path);
            fclose(file);
            return;
        }

        total_bytes_received += bytes_received;

        // Log the progress periodically
        log_message(LOG_INFO, "Received %ld of %ld bytes for file: %s", total_bytes_received, expected_file_size, file_path);
    }

    // Ensure the received file size matches the expected size
    if (total_bytes_received == expected_file_size) {
        log_message(LOG_INFO, "Successfully received complete file: %s, total size: %ld bytes", file_path, total_bytes_received);
    } else {
        log_message(LOG_ERROR, "Incomplete file received: %s. Expected %ld bytes, but got %ld bytes", file_path, expected_file_size, total_bytes_received);
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s -p <port> --source-directory <dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pid_t pid;
    int server_sock, client_sock, port = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char *source_directory = NULL;
    int verbose_mode = 0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--verbose") == 0) || (strcmp(argv[i], "-v") == 0)) {
            verbose_mode = 1;
            set_verbose(verbose_mode);
        } else if ((strcmp(argv[i], "--port") == 0) || (strcmp(argv[i], "-p") == 0)) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--source-directory") == 0) {
            source_directory = argv[++i];
        }
    }

    if (port == 0 || source_directory == NULL) {
        printf("Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }

    // Set the source directory
    strncpy(SRC_DIR, source_directory, sizeof(SRC_DIR));

    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    set_socket_options(server_sock);

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to the specified IP and port
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", port);
    log_message(LOG_INFO, "Server started on port %d", port);

    // Accept and handle client connections in separate processes
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Error accepting client connection");
            log_message(LOG_ERROR, "Error accepting client connection");
            continue;
        }

        // Fork to handle each client in a separate process
        pid = fork();
        if (pid < 0) {
            perror("Error forking process");
            log_message(LOG_ERROR, "Error forking process");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process: handle client
            close(server_sock);
            handle_client(client_sock, client_addr);
            close(client_sock);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process: continue to accept new clients
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}
