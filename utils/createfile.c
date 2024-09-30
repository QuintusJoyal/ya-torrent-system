#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FILENAME 256

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
        fclose(file);
        exit(EXIT_FAILURE);
    }

    generate_random_data(random_data, size);
    fprintf(file, "%s", random_data);

    fclose(file);
    free(random_data);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <directory> <filename> <size_in_bytes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* dir = argv[1];
    const char* filename = argv[2];
    size_t size = (size_t)atoi(argv[3]);

    if (size <= 0) {
        fprintf(stderr, "Invalid file size: %zu\n", size);
        return EXIT_FAILURE;
    }

    // Seed the random number generator
    srand(time(NULL));

    // Create the file with random data
    create_random_file(dir, filename, size);

    printf("File '%s/%s' created with size %zu bytes.\n", dir, filename, size);
    return EXIT_SUCCESS;
}
