# Movie Stream

Movie Stream is a minimal, concurrent HTTP/1.1 server written in C. It is designed to serve static files efficiently and simply, making it ideal for learning about HTTP servers or for lightweight file sharing.

## Features

*   Handles basic `GET` requests (HTTP/1.1)
*   Concurrent client handling using a `fork()`-based model
*   Automatic MIME type detection for served files
*   Simple and minimal codebase for easy understanding and modification
*   Logs basic request information to the console

## Requirements

*   C compiler (e.g., gcc or clang)
*   CMake >= 3.10
*   POSIX-compliant operating system (Linux, macOS, etc.)

## Build Instructions

1. Clone the repository:
    ```bash
    git clone <repository-url>
    cd movie_stream
    ```

2. Configure the project with CMake:
    ```bash
    cmake .
    ```

3. Build the server:
    ```bash
    cmake --build .
    ```

## Run the Server

Start the server on a specific port (e.g., 8080):
By default, the server serves files from the current working directory.

## Notes

*   Only `GET` requests are supported.
*   The server does not support HTTPS or advanced HTTP features.
*   For each incoming connection, a new process is forked to handle the request.
*   MIME types are detected based on file extensions.

## License

This project is licensed under the MIT License.