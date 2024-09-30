#include "protocol.h"
#include "logger.h"
#include "client.h"

int main(int argc, char *argv[]) {
    // Check for the minimum number of arguments
    if (argc < 5) {
        fprintf(stderr, "Usage: %s -h <server_ip> -p <port> --destination-directory <dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = NULL;
    int port = 0;
    int verbose_mode = 0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
            set_verbose(verbose_mode);
        } else if (strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--destination-directory") == 0) {
            strncpy(DEST_DIR, argv[++i], sizeof(DEST_DIR) - 1);
            DEST_DIR[sizeof(DEST_DIR) - 1] = '\0'; // Null-terminate
        }
    }

    // Validate required arguments
    if (server_ip == NULL || port <= 0 || strlen(DEST_DIR) == 0) {
        fprintf(stderr, "Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    int sock = connect_to_server(server_ip, port);
    if (sock < 0) {
        log_message(LOG_ERROR, "Failed to connect to server at %s:%d", server_ip, port);
        exit(EXIT_FAILURE);
    }

    char filename[MAX_FILENAME];
    char input[256];  // Buffer for user input
    int option;

    // Present options to the user in a loop
    while (1) {
        printf("\nChoose an option:\n");
        printf("1. Download a file\n");
        printf("2. Upload a file\n");
        printf("3. View file list\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");

        // Use fgets for input to avoid buffer overflow
        if (!fgets(input, sizeof(input), stdin)) {
            fprintf(stderr, "Error reading input.\n");
            continue; // Continue to the next iteration on error
        }

        // Convert input to an integer option
        sscanf(input, "%d", &option);

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
                log_message(LOG_ERROR, "Invalid option selected: %d", option);
                printf("Invalid option. Please try again.\n");
        }
    }

    // Clean up
    close(sock);
    return 0;
}