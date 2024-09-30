#include "logger.h"
#include <stdarg.h>

#define LOG_FILE "log.txt"  // Define the log file path

static int verbose_mode = 0;  // Flag for verbose mode

// Function to get the current timestamp as a string
const char* get_current_time() {
    static char buffer[20];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// Function to enable verbose logging
void set_verbose(int verbose) {
    verbose_mode = verbose;
}

// Function to log messages with level and timestamp
void log_message(LogLevel level, const char *format, ...) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }

    const char *level_str = (level == LOG_INFO) ? "INFO" : "ERROR";
    fprintf(log_file, "[%s] [%s] ", get_current_time(), level_str);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fclose(log_file);

    // Output to console if verbose mode is enabled
    if (verbose_mode) {
        printf("[%s] [%s] ", get_current_time(), level_str);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}
