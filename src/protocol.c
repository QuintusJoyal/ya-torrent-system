#include "protocol.h"

int send_payload(int sock, Payload *payload) {
    // Send the entire payload structure in one go
    ssize_t sent_bytes = send(sock, payload, sizeof(Payload), 0);
    if (sent_bytes < 0) {
        perror("Error sending payload");
        return -1;
    }
    return 0;
}

int receive_payload(int sock, Payload *payload) {
    // Receive the entire payload structure
    ssize_t received_bytes = recv(sock, payload, sizeof(Payload), 0);
    if (received_bytes <= 0) {
        perror("Error receiving payload");
        return -1;
    }
    return 0;
}

// Function to calculate the hash of a specific chunk of a file
int calculate_file_hash(const char *file_path, long offset, char *hash_output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    FILE *file = fopen(file_path, "rb");
    if (!file) return -1;

    // Read the chunk corresponding to the last completed offset
    fseek(file, offset - CHUNK_SIZE, SEEK_SET);
    char chunk[CHUNK_SIZE];
    fread(chunk, 1, CHUNK_SIZE, file);
    SHA256_Update(&sha256, chunk, CHUNK_SIZE);
    SHA256_Final(hash, &sha256);
    fclose(file);

    // Convert hash to string format
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&hash_output[i * 2], "%02x", hash[i]);
    }

    return 0;
}
