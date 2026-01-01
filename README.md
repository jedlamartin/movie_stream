# Movie Stream

Movie Stream is a minimal, concurrent HTTP/1.1 server written in C. It is designed to serve static files efficiently and simply, making it ideal for learning about HTTP servers or for lightweight file sharing.

## Features

*   Automatically converts `.mkv` files to HLS (`.m3u8` playlists and `.ts` segments) for web playback.
*   Handles basic `GET` requests (HTTP/1.1)
*   Concurrent client handling using a `fork()`-based model
*   Automatic MIME type detection for served files
*   Simple and minimal codebase for easy understanding and modification
*   Logs basic request information to the console

## Requirements

*   C compiler (e.g., gcc or clang)
*   CMake >= 3.10
*   FFmpeg Development Libraries:
    * `libavcodec-dev`
    * `libavformat-dev`
    * `libavutil-dev`
    * `libswscale-dev`
*   POSIX-compliant operating system (Linux, macOS, etc.)


## Build Instructions

1.  **Install Dependencies** (Debian/Ubuntu/Raspberry Pi):
    ```bash
    sudo apt-get update
    sudo apt-get install cmake libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
    ```

2.  **Clone the repository:**
    ```bash
    git clone https://github.com/jedlamartin/movie_stream
    cd movie_stream
    ```

3.  **Configure and Build:**
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## Run the Server

The server accepts command-line arguments to configure the port and connection limits.

```bash
./movie_stream [-p port] [-c max_connections]
```
Start the server on a specific port (e.g., 8080):
By default, the server serves files from the current working directory.

## Notes

*   Only `GET` requests are supported.
*   The server does not support HTTPS or advanced HTTP features.
*   For each incoming connection, a new process is forked to handle the request.
*   MIME types are detected based on file extensions.

## License

This project is licensed under the MIT License.
