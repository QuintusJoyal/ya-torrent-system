#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

// Log levels
typedef enum {
    LOG_INFO,
    LOG_ERROR
} LogLevel;

const char* get_current_time();
void log_message(LogLevel level, const char *format, ...);
void set_verbose(int verbose);

#endif