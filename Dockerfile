# Stage 1: Build the application
FROM gcc:latest AS build

# Set the working directory inside the container
WORKDIR /app

# Copy the source code and Makefile to the container
COPY . .

# Build the application
RUN make clean && make

# Stage 2: Create the deployment image
FROM debian:latest

# Install required libraries for OpenSSL and any runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev \
    && apt-get clean

# Copy the built binaries from the build stage
WORKDIR /app
COPY --from=build /app/bin /app/bin
COPY --from=build /app/tests/bin /app/bin

# Create directories
RUN mkdir -p /app/server_dir /app/client_dir

# Expose the necessary ports for the client and server
EXPOSE 6088