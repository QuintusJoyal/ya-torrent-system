#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "protocol.h"
#include "logger.h"
#include "client.h"

#define MAX_FILENAME 256
#define MAX_FILE_SIZE 65535  // Max size of each file (in bytes)
#define NUM_CLIENTS 10      // Number of clients and files

// Mock server address and port for testing
const char* TEST_SERVER_IP = "127.0.0.1";
const int TEST_SERVER_PORT = 12345;

// Mock directories for testing
const char* CLIENT_DIR = "./client_dir";
const char* SERVER_DIR = "./server_dir";
const char* TEST_FILES[NUM_CLIENTS] = {
    "file1.txt", "file2.txt", "file3.txt", "file4.txt", "file5.txt",
    "file6.txt", "file7.txt", "file8.txt", "file9.txt", "file10.txt"
};

// Helper function to generate random data
void generate_random_data(char* buffer, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t i = 0; i < size - 1; ++i) {
        buffer[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buffer[size - 1] = '\0';  // Null-terminate the string
}

// Helper function to create a random file with random data
void create_random_file(const char* dir, const char* filename, size_t size) {
    char filepath[MAX_FILENAME];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);

    FILE* file = fopen(filepath, "w");
    if (!file) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }

    char* random_data = (char*)malloc(size + 1);
    if (!random_data) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    generate_random_data(random_data, size);
    fprintf(file, "%s", random_data);

    fclose(file);
    free(random_data);
}

// Helper function to compare two files
int compare_files(const char* file1, const char* file2) {
    FILE* f1 = fopen(file1, "rb");
    FILE* f2 = fopen(file2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return -1;  // File(s) could not be opened
    }

    // Check sizes
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    long size1 = ftell(f1);
    long size2 = ftell(f2);
    rewind(f1);
    rewind(f2);

    printf("Comparing sizes: %s (%ld bytes) and %s (%ld bytes)\n", file1, size1, file2, size2);
    if (size1 != size2) {
        fclose(f1);
        fclose(f2);
        return -1;  // Sizes differ
    }

    // Compare contents
    int ch1, ch2;
    do {
        ch1 = fgetc(f1);
        ch2 = fgetc(f2);
    } while (ch1 == ch2 && ch1 != EOF && ch2 != EOF);


    if (ch1 != ch2) {
        printf("Files differ at byte: %ld\n", ftell(f1));
    }

    fclose(f1);
    fclose(f2);

    return (ch1 == ch2) ? 0 : -1;  // Files are identical if ch1 == ch2
}

// Function to generate random files for testing
void generate_test_files() {
    srand(time(NULL));  // Seed the random number generator

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        // Create random files in client_dir
        // create_random_file(CLIENT_DIR, TEST_FILES[i], rand() % MAX_FILE_SIZE + 1);
        
        // Create corresponding random files in server_dir
        create_random_file(SERVER_DIR, TEST_FILES[i], rand() % MAX_FILE_SIZE + 1);
    }

    printf("Random test files generated in both client_dir and server_dir.\n");
}

// Function to simulate a client downloading a file
void simulate_client_download(const char* filename) {
    int sock = connect_to_server(TEST_SERVER_IP, TEST_SERVER_PORT);
    
    download_file(sock, filename);  // Simulate downloading the file

    sleep(1);

    char client_filepath[MAX_FILENAME], server_filepath[MAX_FILENAME];
    snprintf(client_filepath, sizeof(client_filepath), "%s/%s", CLIENT_DIR, filename);
    snprintf(server_filepath, sizeof(server_filepath), "%s/%s", SERVER_DIR, filename);

    // Log to confirm the file was downloaded
    if (access(client_filepath, F_OK) != 0) {
        printf("File not found on client: %s\n", client_filepath);
    } else {
        printf("File downloaded and exists on client: %s\n", client_filepath);
    }

    // Verify that the downloaded file matches the server's version
    assert(compare_files(client_filepath, server_filepath) == 0);

    close(sock);
}

// Function to simulate a client uploading a file
void simulate_client_upload(const char* filename) {
    int sock = connect_to_server(TEST_SERVER_IP, TEST_SERVER_PORT);
    
    upload_file(sock, filename);  // Simulate uploading the file

    sleep(1);

    char client_filepath[MAX_FILENAME], server_filepath[MAX_FILENAME];
    snprintf(client_filepath, sizeof(client_filepath), "%s/%s", CLIENT_DIR, filename);
    snprintf(server_filepath, sizeof(server_filepath), "%s/%s", SERVER_DIR, filename);

    // Log to confirm the file was uploaded
    if (access(server_filepath, F_OK) != 0) {
        printf("File not found on server: %s\n", server_filepath);
    } else {
        printf("File uploaded and exists on server: %s\n", server_filepath);
    }

    assert(compare_files(client_filepath, server_filepath) == 0);

    close(sock);
}

// Function to create client processes for downloading files
void test_simultaneous_download() {
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            simulate_client_download(TEST_FILES[i]);
            exit(0);  // Exit child process
        } else if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        wait(NULL);
    }

    printf("Simultaneous download test passed.\n");
}

// Function to create client processes for uploading files
void test_simultaneous_upload() {
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            simulate_client_upload(TEST_FILES[i]);
            exit(0);  // Exit child process
        } else if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        wait(NULL);
    }

    printf("Simultaneous upload test passed.\n");
}

int main() {
    // Generate random files for the tests
    generate_test_files();

    // Run simultaneous file sharing tests
    test_simultaneous_download();
    test_simultaneous_upload();

    printf("All tests passed!\n");
    return 0;
}
