#include "server.h"
#include "logger.h"
#include "protocol.h"

#include "server.h"
#include "logger.h"
#include "protocol.h"

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s -p <port> --source-directory <dir>\n", argv[0]);
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
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
            set_verbose(verbose_mode);
        } else if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--source-directory") == 0) {
            source_directory = argv[++i];
        }
    }

    if (port <= 0 || source_directory == NULL) {
        fprintf(stderr, "Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }

    // Set the source directory
    strncpy(SRC_DIR, source_directory, sizeof(SRC_DIR) - 1);
    SRC_DIR[sizeof(SRC_DIR) - 1] = '\0';  // Ensure null termination

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
        close(server_sock);  // Close socket before exiting
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Error listening on socket");
        close(server_sock);  // Close socket before exiting
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

        // Log client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);
        log_message(LOG_INFO, "Client connected: IP = %s, Port = %d", client_ip, client_port);

        // Fork to handle each client in a separate process
        pid = fork();
        if (pid < 0) {
            perror("Error forking process");
            log_message(LOG_ERROR, "Error forking process");
            close(client_sock);  // Ensure the client socket is closed
            continue;  // Continue accepting new clients
        } else if (pid == 0) {
            // Child process: handle client
            close(server_sock);  // Child does not need the listening socket
            handle_client(client_sock, client_addr);
            close(client_sock);
            exit(EXIT_SUCCESS);  // Exit child process after handling
        } else {
            // Parent process: continue to accept new clients
            close(client_sock);  // Parent closes the connected socket
        }
    }

    // Cleanup (not reached due to infinite loop)
    close(server_sock);
    return 0;
}
