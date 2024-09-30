#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Log levels
typedef enum {
    LOG_INFO,  ///< Informational messages
    LOG_ERROR  ///< Error messages
} LogLevel;

/**
 * @brief Get the current timestamp as a formatted string.
 *
 * @return A string representing the current time.
 */
const char* get_current_time();

/**
 * @brief Log a message with a specified log level and format.
 *
 * @param level The log level (e.g., LOG_INFO, LOG_ERROR).
 * @param format The format string for the message.
 * @param ... Additional arguments for the format string.
 */
void log_message(LogLevel level, const char *format, ...);

/**
 * @brief Enable or disable verbose logging mode.
 *
 * @param verbose If non-zero, enable verbose mode; otherwise, disable it.
 */
void set_verbose(int verbose);

#endif /* LOGGER_H */
