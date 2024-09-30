#include "server.h"
#include "logger.h"
#include "protocol.h"

char SRC_DIR[MAX_FILENAME] = "server_dir";

// Function to set socket options
void set_socket_options(int sock) {
    int opt = 1;

    // Set SO_REUSEADDR option
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error setting SO_REUSEADDR");
        log_message(LOG_ERROR, "Failed to set SO_REUSEADDR on socket %d", sock);
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEPORT option if supported
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("Error setting SO_REUSEPORT");
        log_message(LOG_ERROR, "Failed to set SO_REUSEPORT on socket %d", sock);
        exit(EXIT_FAILURE);
    }
#endif

    log_message(LOG_INFO, "Socket options set successfully on socket %d", sock);
}

// Function to handle client connections
void handle_client(int client_sock, struct sockaddr_in client_addr) {
    Payload payload;

    // Infinite loop to continuously handle requests
    while (1) {
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
                goto cleanup;  // Exit to cleanup directly

            default:
                log_message(LOG_ERROR, "Invalid operation received from client: %d", payload.operation);
                printf("Invalid operation received from client\n");
                break;
        }
    }

cleanup:
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

    // Retrieve file statistics
    if (stat(file_path, &file_stat) != 0) {
        // File not found; send STAT_FILE_NOT_FOUND
        log_message(LOG_ERROR, "File not found: %s", filename);
        
        // Initialize the payload for file not found status
        memset(&metadata_payload, 0, sizeof(metadata_payload));
        metadata_payload.operation = OP_META_DATA; // Set appropriate operation
        memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename));
        metadata_payload.status = STAT_FILE_NOT_FOUND; // Set status to file not found

        // Send the metadata payload to the client
        if (send_payload(client_sock, &metadata_payload) != 0) {
            log_message(LOG_ERROR, "Failed to send file not found status for: %s", filename);
        } else {
            log_message(LOG_INFO, "Sent file not found status for file: %s", filename);
        }
        return; // Exit the function after sending the error status
    }

    long file_size = file_stat.st_size;
    char hash[HASH_SIZE];

    // Initialize the metadata payload
    memset(&metadata_payload, 0, sizeof(metadata_payload));
    metadata_payload.operation = OP_META_DATA;
    memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename));
    metadata_payload.file_size = file_size;

    // Handle the hash calculation based on the offset
    if (offset > 0) {
        if (calculate_file_hash(file_path, offset, hash) == 0) {
            memcpy(metadata_payload.hash, hash, sizeof(metadata_payload.hash));
            metadata_payload.status = STAT_FILE_VERIFY;
        } else {
            log_message(LOG_ERROR, "Error calculating hash for file '%s'", file_path);
            return;
        }
    } else {
        // No offset means this is a new upload; send an empty hash
        metadata_payload.status = STAT_FILE_FOUND;
    }

    // Send the metadata payload to the client
    if (send_payload(client_sock, &metadata_payload) != 0) {
        log_message(LOG_ERROR, "Failed to send metadata for file: %s", filename);
    } else {
        log_message(LOG_INFO, "Sent metadata for file: %s with hash", filename);
    }
}

// Function to send request for metadata
void send_request_metadata(int client_sock, const char *filename) {
    Payload req_payload;
    memset(&req_payload, 0, sizeof(req_payload));  // Clear the payload structure

    // Set the operation type and filename in the payload
    req_payload.operation = OP_REQ_META_DATA;
    strncpy(req_payload.filename, filename, sizeof(req_payload.filename) - 1);
    req_payload.filename[sizeof(req_payload.filename) - 1] = '\0';  // Ensure null-termination

    // Send the payload to the client
    if (send_payload(client_sock, &req_payload) != 0) {
        log_message(LOG_ERROR, "Failed to send metadata request for file: %s", filename);
        return;  // Exit if sending fails
    }

    log_message(LOG_INFO, "Requested metadata for file: %s", filename);
}

// Function to send the list of available files in the shared directory
void send_file_list(int client_sock) {
    DIR *dir;
    struct dirent *entry;
    char file_list[1024] = "";  // Buffer to hold the file list

    dir = opendir(SRC_DIR);
    if (dir == NULL) {
        log_message(LOG_ERROR, "Error opening shared directory: %s", strerror(errno));
        const char *error_message = "Error opening shared directory.\n";
        send(client_sock, error_message, strlen(error_message), 0);
        return;  // Exit the function on error
    }

    while ((entry = readdir(dir)) != NULL) {
        // Only consider regular files
        if (entry->d_type == DT_REG) {  
            // Check if adding this file name would exceed the buffer size
            size_t new_length = strlen(file_list) + strlen(entry->d_name) + 2; // +2 for newline and null-terminator
            if (new_length < sizeof(file_list)) {
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
            } else {
                log_message(LOG_INFO, "File list too long, truncating");
                break;  // Stop adding more files to avoid overflow
            }
        }
    }

    closedir(dir);

    // Check if the file list is empty and send an appropriate message
    if (strlen(file_list) == 0) {
        const char *no_files_message = "No files available in the shared directory.\n";
        send(client_sock, no_files_message, strlen(no_files_message), 0);
    } else {
        // Send the file list directly
        if (send(client_sock, file_list, strlen(file_list), 0) < 0) {
            log_message(LOG_ERROR, "Error sending file list to client: %s", strerror(errno));
        } else {
            log_message(LOG_INFO, "Sent file list to client.");
        }
    }
}

// Function to send a file in chunks from a specific offset
void send_file(int client_sock, const char *filename, long offset) {
    char file_path[MAX_FILENAME];
    // Form the full file path
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", SRC_DIR, filename);

    // Check for snprintf errors
    if (result < 0 || result >= sizeof(file_path)) {
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Open the file for reading
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        log_message(LOG_ERROR, "Error opening file: %s", file_path);
        return;
    }

    // Move the file pointer to the desired offset
    if (fseek(file, offset, SEEK_SET) != 0) {
        log_message(LOG_ERROR, "Error seeking to offset %ld in file: %s", offset, file_path);
        fclose(file);
        return;
    }

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;

    // Read and send the file in chunks
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        ssize_t bytes_sent = 0;

        // Ensure the entire buffer is sent
        while (bytes_sent < bytes_read) {
            ssize_t send_result = send(client_sock, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (send_result < 0) {
                log_message(LOG_ERROR, "Error sending file: %s", file_path);
                fclose(file);
                return;
            }
            bytes_sent += send_result;
        }
    }

    // Check for reading errors
    if (ferror(file)) {
        log_message(LOG_ERROR, "Error reading file: %s", file_path);
    } else {
        log_message(LOG_INFO, "Successfully sent file: %s from offset: %ld", file_path, offset);
    }

    fclose(file);
}

// Function to receive a file from the client and save it (with overwrite and reliability)
void receive_file(int client_sock, const char *filename, long expected_file_size) {
    char file_path[MAX_FILENAME];
    
    // Form the file path for saving the received file
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", SRC_DIR, filename);
    if (result < 0 || result >= sizeof(file_path)) {
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Open the file in "wb" mode to overwrite if it exists
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        log_message(LOG_ERROR, "Error creating file: %s", file_path);
        return;
    }

    char buffer[CHUNK_SIZE];
    long total_bytes_received = 0;

    // Loop to receive chunks until the full file is received
    while (total_bytes_received < expected_file_size) {
        // Calculate the size of the chunk to receive
        long bytes_remaining = expected_file_size - total_bytes_received;
        int chunk_size = (bytes_remaining < CHUNK_SIZE) ? bytes_remaining : CHUNK_SIZE;

        // Receive the next chunk
        ssize_t bytes_received = recv(client_sock, buffer, chunk_size, 0);
        if (bytes_received <= 0) {
            // Log closure or error details
            if (bytes_received == 0) {
                log_message(LOG_INFO, "Connection closed by client before full file was received");
            } else {
                log_message(LOG_ERROR, "Error receiving file: %s", file_path);
            }
            fclose(file);
            return;
        }

        // Write the received chunk to the file
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            log_message(LOG_ERROR, "Error writing to file: %s", file_path);
            fclose(file);
            return;
        }

        total_bytes_received += bytes_received;

        // Log the progress
        log_message(LOG_INFO, "Received %ld of %ld bytes for file: %s", total_bytes_received, expected_file_size, file_path);
    }

    // Final verification of received file size
    if (total_bytes_received == expected_file_size) {
        log_message(LOG_INFO, "Successfully received complete file: %s, total size: %ld bytes", file_path, total_bytes_received);
    } else {
        log_message(LOG_ERROR, "Incomplete file received: %s. Expected %ld bytes, but got %ld bytes", file_path, expected_file_size, total_bytes_received);
    }

    fclose(file);
}
