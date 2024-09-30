#include "protocol.h"
#include "logger.h"
#include "client.h"

char DEST_DIR[MAX_FILENAME];

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
        log_message(LOG_ERROR, "Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Connection failed");
        exit(EXIT_FAILURE);
    }

    log_message(LOG_INFO, "Connected to server %s:%d", server_ip, port);
    return sock;
}

// Function to request and display the file list from the server
void request_file_list(int sock) {
    Payload payload;
    memset(&payload, 0, sizeof(payload));

    payload.operation = OP_LIST_FILES;

    // Send the payload to request the file list
    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Error requesting file list");
        return;
    }

    // Receive the list of files from the server
    char buffer[1024] = {0};
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        log_message(LOG_ERROR, "Error receiving file list");
        return;
    }

    // Check if the received file list is empty
    buffer[bytes_received] = '\0';  // Null-terminate the received string
    if (strlen(buffer) == 0 || (strlen(buffer) == 1 && buffer[0] == '\n')) {
        printf("No files available in the shared directory.\n");
    } else {
        printf("Available files:\n%s", buffer);
    }
}

// Function to download a file from the server with hash validation
void download_file(int sock, const char *filename) {
    char buffer[CHUNK_SIZE];
    char file_path[MAX_FILENAME];
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

        // Ensure the resume offset is aligned to a chunk boundary
        long remainder = resume_offset % CHUNK_SIZE;
        if (remainder != 0) {
            resume_offset -= remainder;
        }
    }

    // Request file metadata from the server (including size and hash validation)
    Payload metadata;
    if (request_file_metadata(sock, filename, resume_offset, &metadata) != 0) {
        log_message(LOG_ERROR, "Failed to get file metadata from server");
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
        log_message(LOG_ERROR, "Error opening file for download");
        return;
    }

    // Move the file pointer to the resume offset if continuing
    if (resume_offset > 0) {
        if (fseek(file, resume_offset, SEEK_SET) != 0) {
            log_message(LOG_ERROR, "Error seeking to resume offset %ld", resume_offset);
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
        log_message(LOG_ERROR, "Failed to send download request");
        fclose(file);
        return;
    }

    // Download the file in chunks
    int bytes_received;
    long total_downloaded = resume_offset;
    while (total_downloaded < total_size && (bytes_received = recv(sock, buffer, CHUNK_SIZE, 0)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            log_message(LOG_ERROR, "Error writing to file");
            break;
        }
        total_downloaded += bytes_received;

        // Update the progress display
        display_progress(total_size, total_downloaded);
    }

    // Check for errors or incomplete download
    if (bytes_received < 0) {
        log_message(LOG_ERROR, "Error during download");
    } else if (total_downloaded == total_size) {
        log_message(LOG_INFO, "Download complete for '%s'", filename);
    } else {
        log_message(LOG_INFO, "Download interrupted. Downloaded %ld of %ld bytes", total_downloaded, total_size);
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
                log_message(LOG_ERROR, "Failed to calculate hash for local file: %s", filename);
                return -1;
            }
        }
    }
    return 0;
}

// Function to upload a file to the server
void upload_file(int sock, const char *filename) {
    char buffer[CHUNK_SIZE];
    char file_path[MAX_FILENAME];

    int result = snprintf(file_path, sizeof(file_path), "%s/%s", DEST_DIR, filename);
    if (result < 0 || result >= sizeof(file_path)) {
        perror("Error forming file path");
        log_message(LOG_ERROR, "Error forming file path for filename: %s", filename);
        return;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        log_message(LOG_ERROR, "Error opening file for upload: %s", filename);
        return;
    }

    // Prepare and send the upload request
    Payload payload;
    memset(&payload, 0, sizeof(payload));

    payload.operation = OP_UPLOAD;

    memcpy(payload.filename, filename, sizeof(payload.filename));


    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send upload request");
        fclose(file);
        return;
    }

    Payload req_payload;
    memset(&req_payload, 0, sizeof(req_payload));


    if (receive_payload(sock, &req_payload) != 0 && req_payload.operation != OP_REQ_META_DATA) {
        log_message(LOG_ERROR, "Failed to receive metadata request");
        fclose(file);
        return;
    }

    Payload metadata_payload;
    struct stat file_stat;

    if (stat(file_path, &file_stat) == 0) {
        long file_size = file_stat.st_size;
        memset(&metadata_payload, 0, sizeof(metadata_payload));

        metadata_payload.operation = OP_META_DATA;
        memcpy(metadata_payload.filename, filename, sizeof(metadata_payload.filename));
        metadata_payload.file_size = file_size;

        send_payload(sock, &metadata_payload);
        log_message(LOG_INFO, "Sent metadata for file: %s with hash", filename);
    } else {
        log_message(LOG_ERROR, "Error retrieving file metadata for %s", filename);
    }

    // Upload file in chunks
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (send(sock, buffer, bytes_read, 0) < 0) {
            log_message(LOG_ERROR, "Error sending file: %s", filename);
            fclose(file);
            return;
        }
    }

    printf("File upload complete for '%s'\n", filename);
    log_message(LOG_INFO, "File upload complete for '%s'", filename);

    fclose(file);
}

// Function to send an exit request to the server
void send_exit_request(int sock) {
    Payload payload;
    memset(&payload, 0, sizeof(payload));

    payload.operation = OP_EXIT;

    if (send_payload(sock, &payload) != 0) {
        log_message(LOG_ERROR, "Failed to send exit request");
    } else {
        log_message(LOG_INFO, "Exit request sent to server");
    }
}

// Function to display the download progress
void display_progress(long total_size, long downloaded) {
    int progress = (int)((double)downloaded / total_size * 100);
    printf("\rProgress: %d%% (%ld/%ld bytes)", progress, downloaded, total_size);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s -h <server_ip> -p <port> --destination-directory <dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = NULL;
    int port = 0;
    char *destination_directory = NULL;
    int verbose_mode = 0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--verbose") == 0) || (strcmp(argv[i], "-v") == 0)) {
            verbose_mode = 1;
            set_verbose(verbose_mode);
        } else if ((strcmp(argv[i], "--host") == 0) || (strcmp(argv[i], "-h") == 0)) {
            server_ip = argv[++i];
        } else if ((strcmp(argv[i], "--port") == 0) || (strcmp(argv[i], "-p") == 0)) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--destination-directory") == 0) {
            destination_directory = argv[++i];
        }
    }

    if (server_ip == NULL || port == 0 || destination_directory == NULL) {
        printf("Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }

    // Set the destination directory
    strncpy(DEST_DIR, destination_directory, sizeof(DEST_DIR));

    int sock = connect_to_server(server_ip, port);
    char filename[MAX_FILENAME];
    char input[256];  // Buffer for user input
    int option;

    // Present options to the user
    while (1) {
        printf("\nChoose an option:\n");
        printf("1. Download a file\n");
        printf("2. Upload a file\n");
        printf("3. View file list\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        
        fgets(input, sizeof(input), stdin);  // Use fgets for input
        sscanf(input, "%d", &option);  // Convert to int

        switch (option) {
            case 1:  // Download file
                request_file_list(sock);
                printf("Enter the file name to download: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';  // Remove newline character

                download_file(sock, filename);
                break;

            case 2:  // Upload file
                printf("Enter the file name to upload: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';  // Remove newline character
                upload_file(sock, filename);
                break;

            case 3:  // View file list
                request_file_list(sock);
                break;

            case 4:  // Exit
                printf("Exiting the program.\n");
                send_exit_request(sock);  // Send exit request to server
                close(sock);
                return 0;

            default:
                log_message(LOG_ERROR, "Invalid option selected");
                printf("Invalid option. Please try again.\n");
        }
    }

    close(sock);
    return 0;
}
