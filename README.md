# Movie Stream

A minimal, concurrent HTTP/1.1 server written in C. It handles `GET` requests and serves static files using a forking model.

## Features

*   Handles basic `GET` requests (HTTP/1.1)
*   Concurrent client handling using `fork()`
*   Automatic MIME type detection

## Build & Run

```bash
cmake --build .
./movie_stream -p 8080
