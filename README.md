# ya-torrent-system

## Introduction

The `ya-torrent-system` is a simplified implementation of a torrent file exchange scenario, designed to demonstrate socket programming in C. This project allows clients to upload and download files from a server, mimicking the behavior of a torrent system. Key features include hashing for data integrity, logging for debugging, and a robust command-line interface for configuration.

## Table of Contents

- [Features](#features)
- [File Structure](#file-structure)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Build and Run](#build-and-run)
- [Create File Utility](#create-file-utility)
- [Configuration](#configuration)
- [License](#license)

## Features

- File upload and download functionality
- Hashing for data integrity and resuming interrupted downloads
- Logging for monitoring and debugging
- Support for command-line arguments to configure server and client behavior
- Utility to create files with specified names and sizes

## File Structure

```
├── Dockerfile
├── include
│   ├── client.h
│   ├── logger.h
│   ├── protocol.h
│   └── server.h
├── Makefile
├── README.md
├── src
│   ├── cli2219.c
│   ├── client.c
│   ├── logger.c
│   ├── protocol.c
│   ├── server.c
│   └── srv6088.c
├── tests
│   └── test_client.c
└── utils
    └── createfile.c

```

## Getting Started

### Prerequisites

- C Compiler (gcc)
- Make
- OpenSSL libraries

### Build and Run

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd ya-torrent-system
   ```

2. Build the project using Make:
   ```bash
   make
   ```

3. Run the server:
   ```bash
   ./bin/srv6088 --port 6088 --source-directory /path/to/source
   ```

4. Run the client:
   ```bash
   ./bin/cli2219 --host 127.0.0.1 --port 6088 --destination-directory /path/to/destination
   ```

## Usage

The client and server can be run with the following command-line arguments:

### Client Arguments
- `--verbose` or `-v`: Enable verbose logging
- `--host` or `-h`: Specify the server IP address
- `--port` or `-p`: Specify the server port
- `--destination-directory`: Set the directory to save downloaded files

### Server Arguments
- `--verbose` or `-v`: Enable verbose logging
- `--port` or `-p`: Specify the server port
- `--source-directory`: Set the directory to look for files to serve

## Create File Utility

The project includes a utility to create files with specified names and sizes. This utility can be used for testing file uploads and downloads.

### Usage

To create a file, run the following command:

```bash
./bin/createfile <directory> <filename> <size>
```

- `<directory>`: The directory where the file will be created.
- `<filename>`: The name of the file to be created.
- `<size>`: The size of the file in bytes.

### Example

To create a file named `example.txt` of size `1024` bytes in the `./test_files` directory, use the command:

```bash
./bin/createfile ./test_files example.txt 1024
```
