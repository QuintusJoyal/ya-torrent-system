#include "protocol.h"
#include "logger.h"
#include "client.h"

char DEST_DIR[MAX_FILENAME] = "client_dir";

// Function to connect to the server
int connect_to_server(const char *server_ip, int port) {
    int sock;
    struct sockaddr_in server_addr;

    // Create the socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message(LOG_ERROR, "Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        log_message(LOG_ERROR, "Invalid address/Address not supported: %s", server_ip);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Connection failed to %s:%d", server_ip, port);
        exit(EXIT_FAILURE);
    }

    log_message(LOG_INFO, "Successfully connected to server %s:%d", server_ip, port);
    return sock;
}

// Function to request and display the file list from the server
void request_file_list(int sock) {
    Payload payload;
    memset(&payload, 0, sizeof(payload));

    payload.operation = OP_LIST_FILES;

    // Send the payload to request the file list
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Error requesting file list from server");
        return;
    }

    // Receive the list of files from the server
    char buffer[1024] = {0};
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        log_message(LOG_ERROR, "Error receiving file list from server");
        return;
    }

    // Null-terminate the received string
    buffer[bytes_received] = '\0';  
    // Check if the received file list is empty
    if (strlen(buffer) == 0 || (strlen(buffer) == 1 && buffer[0] == '\n')) {
        printf("No files available in the shared directory.\n");
    } else {
        printf("Available files:\n%s", buffer);
        log_message(LOG_INFO, "Received file list: %s", buffer);
    }
}

// Function to download a file from the server with hash validation
void download_file(int sock, const char *filename) {
    char buffer[CHUNK_SIZE];
    char file_path[MAX_FILENAME];

    // Construct the file path
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", DEST_DIR, filename);
    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Check if the file already exists and determine resume offset
    long resume_offset = 0;
    FILE *file = fopen(file_path, "rb");
    if (file) {
        // Get the size of the partially downloaded file
        fseek(file, 0, SEEK_END);
        resume_offset = ftell(file);
        fclose(file);
        
        // Log the resume offset
        log_message(LOG_INFO, "Resuming download for '%s' at offset %ld", filename, resume_offset);

        // Ensure the resume offset is aligned to a chunk boundary
        long remainder = resume_offset % CHUNK_SIZE;
        if (remainder != 0) {
            resume_offset -= remainder; // Align to chunk size
            log_message(LOG_INFO, "Adjusted resume offset to %ld for chunk alignment", resume_offset);
        }
    }

    // Request file metadata from the server (including size and hash validation)
    Payload metadata;
    if (request_file_metadata(sock, filename, resume_offset, &metadata) != 0) {
        log_message(LOG_ERROR, "Failed to get file metadata from server for '%s'", filename);
        return;
    }

    // Handle different server responses for file status
    if (metadata.status == STAT_FILE_NOT_FOUND) {
        log_message(LOG_INFO, "File '%s' not found on server", filename);
        return;
    } else if (metadata.status == STAT_FILE_CHANGED || metadata.status == STAT_FILE_FOUND) {
        log_message(LOG_INFO, "File has changed on the server. Starting a fresh download.");
        resume_offset = 0;  // Reset to download from the beginning
    } else {
        log_message(LOG_INFO, "Resuming download from offset %ld", resume_offset);
    }

    // Open the file for writing (resuming or starting fresh)
    file = fopen(file_path, resume_offset > 0 ? "r+b" : "wb");
    if (!file) {
        log_message(LOG_ERROR, "Error opening file for download: %s", filename);
        return;
    }

    // Move the file pointer to the resume offset if continuing
    if (resume_offset > 0) {
        if (fseek(file, resume_offset, SEEK_SET) != 0) {
            log_message(LOG_ERROR, "Error seeking to resume offset %ld for '%s'", resume_offset, filename);
            fclose(file);
            return;
        }
    }

    // Get the total file size from server metadata
    long total_size = metadata.file_size;

    // Prepare the download request payload
    Payload payload = {0};
    payload.operation = OP_DOWNLOAD;
    strncpy(payload.filename, filename, sizeof(payload.filename));
    payload.offset = resume_offset;

    // Send the download request
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send download request for '%s'", filename);
        fclose(file);
        return;
    }

    // Download the file in chunks
    int bytes_received;
    long total_downloaded = resume_offset;
    while (total_downloaded < total_size && (bytes_received = recv(sock, buffer, CHUNK_SIZE, 0)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            log_message(LOG_ERROR, "Error writing to file '%s'", filename);
            break;
        }
        total_downloaded += bytes_received;

        // Update the progress display
        display_progress(total_size, total_downloaded);
    }

    // Check for errors or incomplete download
    if (bytes_received < 0) {
        log_message(LOG_ERROR, "Error during download of '%s'", filename);
    } else if (total_downloaded == total_size) {
        log_message(LOG_INFO, "Download complete for '%s'", filename);
    } else {
        log_message(LOG_INFO, "Download interrupted for '%s'. Downloaded %ld of %ld bytes", filename, total_downloaded, total_size);
    }

    fclose(file);
}

// Function to request file metadata from the server and compare the hash locally
int request_file_metadata(int sock, const char *filename, long offset, Payload *metadata) {
    // Prepare payload to request file metadata
    Payload payload;
    memset(&payload, 0, sizeof(payload));

    payload.operation = OP_REQ_META_DATA;
    strncpy(payload.filename, filename, sizeof(payload.filename));
    payload.offset = offset;

    // Send the payload to the server
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send metadata request for '%s'", filename);
        return -1;
    }

    // Receive metadata response from the server
    if (receive_payload(sock, metadata) != 0) {
        log_message(LOG_ERROR, "Error receiving metadata for '%s'", filename);
        return -1;
    }

    // Log received metadata
    log_message(LOG_INFO, "Received metadata for '%s': size=%ld, hash=%s", 
                filename, metadata->file_size, metadata->hash);

    // Check if file exists locally and compare hashes
    char file_path[MAX_FILENAME];
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", DEST_DIR, filename);

    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return -1;
    }

    char local_hash[HASH_SIZE] = {0};

    // If the file exists, calculate its hash
    if (metadata->status == STAT_FILE_VERIFY) {
        FILE *file = fopen(file_path, "rb");
        if (file) {
            fclose(file);
            
            // Calculate the local file's hash
            if (calculate_file_hash(file_path, offset, local_hash) == 0) {
                log_message(LOG_INFO, "Local file hash: %s", local_hash);

                // Compare the local hash with the server hash
                if (strcmp(local_hash, metadata->hash) == 0) {
                    log_message(LOG_INFO, "File '%s' is unchanged on the server", filename);
                    metadata->status = STAT_FILE_UNCHANGED;
                } else {
                    log_message(LOG_INFO, "File '%s' has changed on the server", filename);
                    metadata->status = STAT_FILE_CHANGED;
                }
            } else {
                log_message(LOG_ERROR, "Error calculating hash for local file '%s'", filename);
            }
        } else {
            log_message(LOG_INFO, "File '%s' does not exist locally, downloading fresh", filename);
            metadata->status = STAT_FILE_FOUND;  // File not found
        }
    }

    return 0;
}

// Function to upload a file to the server
void upload_file(int sock, const char *filename) {
    char buffer[CHUNK_SIZE];         // Buffer to hold file data chunks
    char file_path[MAX_FILENAME];    // Full file path for the file to upload

    // Construct the full file path from the destination directory and filename
    int result = snprintf(file_path, sizeof(file_path), "%s/%s", DEST_DIR, filename);
    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    // Open the file for reading in binary mode
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        log_message(LOG_ERROR, "Error opening file for upload: %s", filename);
        return;
    }

    // Prepare the upload request payload
    Payload payload;
    memset(&payload, 0, sizeof(payload));
    payload.operation = OP_UPLOAD; // Set operation type to upload
    memcpy(payload.filename, filename, sizeof(payload.filename)); // Copy filename to payload

    // Send the upload request to the server
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send upload request for '%s'", filename);
        fclose(file);
        return;
    }

    // Receive a request for file metadata from the server
    Payload req_payload;
    memset(&req_payload, 0, sizeof(req_payload));

    if (receive_payload(sock, &req_payload) != 0 && req_payload.operation != OP_REQ_META_DATA) {
        log_message(LOG_ERROR, "Failed to receive metadata request from server for '%s'", filename);
        fclose(file);
        return;
    }

    // Prepare to send file metadata (size)
    Payload metadata_payload;
    struct stat file_stat;

    // Retrieve file metadata (size)
    if (stat(file_path, &file_stat) == 0) {
        long file_size = file_stat.st_size; // Get the size of the file
        memset(&metadata_payload, 0, sizeof(metadata_payload));

        // Set up metadata payload with file information
        metadata_payload.operation = OP_META_DATA; // Set operation type to metadata
        memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename)); // Copy filename
        metadata_payload.file_size = file_size; // Set file size

        // Send the file metadata to the server
        if (send_payload(sock, &metadata_payload) != 0) {
            log_message(LOG_ERROR, "Failed to send metadata for file: %s", filename);
            fclose(file);
            return;
        }

        log_message(LOG_INFO, "Sent metadata for file: %s, size: %ld bytes", filename, file_size);
    } else {
        log_message(LOG_ERROR, "Error retrieving file metadata for %s", filename);
        fclose(file);
        return;
    }

    // Upload file in chunks
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        // Send the read chunk to the server
        if (send(sock, buffer, bytes_read, 0) < 0) {
            log_message(LOG_ERROR, "Error sending file chunk for: %s", filename);
            fclose(file);
            return;
        }
        log_message(LOG_INFO, "Uploaded %d bytes of file '%s'", bytes_read, filename);
    }

    // Close the file after uploading
    fclose(file);

    // Log completion of the upload
    printf("File upload complete for '%s'\n", filename);
    log_message(LOG_INFO, "File upload complete for '%s'", filename);
}

// Function to send an exit request to the server
void send_exit_request(int sock) {
    Payload payload;
    memset(&payload, 0, sizeof(payload)); // Clear the payload structure

    payload.operation = OP_EXIT; // Set the operation type to exit

    // Attempt to send the exit request payload to the server
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send exit request to server");
    } else {
        log_message(LOG_INFO, "Exit request successfully sent to server");
    }
}

// Function to display the download progress
void display_progress(long total_size, long downloaded) {
    if (total_size <= 0) {
        log_message(LOG_ERROR, "Invalid total size for progress display: %ld", total_size);
        return; // Avoid division by zero or invalid progress display
    }

    // Calculate the progress percentage
    int progress = (int)((double)downloaded / total_size * 100);
    printf("\rProgress: %d%% (%ld/%ld bytes)", progress, downloaded, total_size);
    fflush(stdout); // Ensure the output is displayed immediately
}