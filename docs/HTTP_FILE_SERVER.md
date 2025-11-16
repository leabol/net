# HTTP File Server

This document describes the lightweight HTTP service built on top of the existing `TcpServer` stack. The server exposes a REST-style API plus a minimal HTML dashboard for listing, uploading, downloading, and deleting files from a configurable storage directory.

## Build

From the repository root run the regular build (the `http_file_server` target and its supporting `http_server` library are part of the default build graph):

```bash
cmake --build build -j
```

## Run

The executable accepts optional arguments: `<port> [storageDir] [staticDir]`.

```bash
./build/http_file_server 9200 storage www
```

- `port` – TCP port to bind (defaults to `9200`).
- `storageDir` – directory used to persist uploaded files (defaults to `storage`).
- `staticDir` – directory serving the dashboard assets (defaults to `www`).

The server ensures the storage directory exists; static assets are read as-is, so keep `www/index.html` in sync with UI needs.

## API Surface

All responses are UTF-8. Unless explicitly mentioned, connections are closed after each response.

| Method | Path                 | Description                                                    |
| ------ | -------------------- | -------------------------------------------------------------- |
| GET    | `/`                  | Serves the HTML dashboard (`index.html`).                      |
| GET    | `/api/files`         | Returns `{"files": ["name", ...]}` listing storage contents. |
| GET    | `/api/files/{name}`  | Streams the specified file with `Content-Disposition`.         |
| POST   | `/api/files`         | Uploads raw bytes from the request body. Requires `X-Filename` header (URL-encoded filename). |
| DELETE | `/api/files/{name}`  | Removes the specified file if it exists.                       |

### Upload Contract

The frontend (see below) sends binary bodies with `Content-Type: application/octet-stream` and an `X-Filename` header carrying `encodeURIComponent(file.name)`. The server decodes, sanitizes, and writes the file atomically to the storage directory.

## HTML Dashboard

A static page lives in `www/index.html`. It:

- Lists the current files via `GET /api/files`.
- Uploads the selected file via `POST /api/files`.
- Provides download and delete buttons per file row.
- Displays inline status messages for quick feedback.

You can replace the HTML with any other frontend as long as it honors the API contract above.

## Notes & Limitations

- The request parser is intentionally simple: it requires a `Content-Length` header and closes the connection after each response (no keep-alive).
- Filenames are sanitized to avoid directory traversal. Conflicting uploads overwrite existing files.
- Error responses are plain text with appropriate HTTP status codes; extend `HttpServer` if you need richer metadata.
