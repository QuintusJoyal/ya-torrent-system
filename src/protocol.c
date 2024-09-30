#include "protocol.h"

// Function to send the payload structure
int send_payload(int sock, const Payload *payload) {
    // Send the entire payload structure in one go
    ssize_t sent_bytes = send(sock, payload, sizeof(Payload), 0);
    if (sent_bytes < 0) {
        perror("Error sending payload");
        return -1;
    } else if (sent_bytes < sizeof(Payload)) {
        fprintf(stderr, "Partial payload sent: %zd of %zu bytes\n", sent_bytes, sizeof(Payload));
        return -1;  // Indicate a partial send failure
    }
    return 0;  // Successful send
}

// Function to receive the payload structure
int receive_payload(int sock, Payload *payload) {
    // Receive the entire payload structure
    ssize_t received_bytes = recv(sock, payload, sizeof(Payload), 0);
    if (received_bytes <= 0) {
        perror("Error receiving payload");
        return -1;
    } else if (received_bytes < sizeof(Payload)) {
        fprintf(stderr, "Partial payload received: %zd of %zu bytes\n", received_bytes, sizeof(Payload));
        return -1;  // Indicate a partial receive failure
    }
    return 0;  // Successful receive
}

// Function to calculate the hash of a specific chunk of a file
int calculate_file_hash(const char *file_path, long offset, char *hash_output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    // Open the file for reading
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file for hashing");
        return -1;  // Return error if file cannot be opened
    }

    // Ensure the offset is valid
    if (offset < CHUNK_SIZE) {
        fprintf(stderr, "Invalid offset: %ld for file: %s\n", offset, file_path);
        fclose(file);
        return -1;  // Invalid offset
    }

    // Read the chunk corresponding to the last completed offset
    if (fseek(file, offset - CHUNK_SIZE, SEEK_SET) != 0) {
        perror("Error seeking to offset in file");
        fclose(file);
        return -1;  // Return error if seeking fails
    }

    char chunk[CHUNK_SIZE];
    size_t bytes_read = fread(chunk, 1, CHUNK_SIZE, file);
    if (bytes_read < CHUNK_SIZE) {
        if (feof(file)) {
            fprintf(stderr, "End of file reached while reading chunk: %s\n", file_path);
        } else {
            perror("Error reading file chunk");
        }
        fclose(file);
        return -1;  // Return error if reading fails
    }

    // Update SHA256 context with the chunk
    SHA256_Update(&sha256, chunk, CHUNK_SIZE);
    SHA256_Final(hash, &sha256);
    fclose(file);

    // Convert hash to string format
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&hash_output[i * 2], "%02x", hash[i]);
    }

    return 0;  // Successful hash calculation
}