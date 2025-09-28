# Roadmap: Building a Full HTTP Server in C

This is a step-by-step roadmap for building a complete HTTP/1.1 + HTTPS server in C, starting from basic socket programming.

---

## Phase 1 — Core Networking Foundation

1. **Socket basics**

   - Create TCP socket (`socket()`)
   - Bind to port 80 (or >1024 if non-root) (`bind()`)
   - Listen for connections (`listen()`)
   - Accept one client (`accept()`)
   - Read data (`recv()`)
   - Send raw response (`send()`)

2. **Loop for multiple sequential clients**
   - Wrap `accept()` in a loop so the server handles one connection, closes it, then waits for another.

---

## Phase 2 — Minimal HTTP Request Handling

3. **Implement request parsing (basic)**

   - Read request line (e.g., `GET /index.html HTTP/1.1`)
   - Extract: method, path, version
   - Read headers line by line until empty line (`\r\n\r\n`)
   - Store headers in a map/dictionary structure

4. **Error handling for malformed requests**

   - Invalid start line → respond `400 Bad Request`
   - Missing `Host` header in HTTP/1.1 → `400 Bad Request`
   - Unsupported HTTP version → `505 HTTP Version Not Supported`

5. **Static response logic**
   - Map requested path → a file on disk (e.g., `/index.html` → `./www/index.html`)
   - If file exists, return `200 OK` + contents
   - If not, return `404 Not Found`

---

## Phase 3 — Standards Compliance

6. **HTTP response formatting**

   - Correctly output status line (e.g., `HTTP/1.1 200 OK`)
   - Add essential headers:
     - `Date:` (RFC1123 format)
     - `Content-Type:` (guess from file extension)
     - `Content-Length:` (bytes in body)
     - `Connection: close` or `keep-alive`

7. **Support for different methods**

   - GET (mandatory)
   - HEAD (headers only, no body)
   - POST (accept request body, echo/store it for now)

8. **HTTP/1.1 keep-alive support**
   - Parse `Connection:` header
   - If `keep-alive`, reuse socket for multiple requests
   - Implement timeout for idle connections

---

## Phase 4 — Concurrency & Scalability

9. **Concurrent clients**

   - Option A: `fork()` per client
   - Option B: threads with `pthread_create()`
   - Option C: non-blocking I/O with `select()`, `poll()`, or `epoll()`

10. **Thread safety**
    - Synchronize logging, shared resources
    - Handle race conditions in concurrent file access

---

## Phase 5 — Advanced Features

11. **MIME type handling**

    - Lookup table for common extensions (html, css, js, jpg, png, etc.)
    - Return correct `Content-Type` header

12. **Directory listing (optional)**

    - If directory requested and no `index.html` exists, list files

13. **CGI or dynamic content (optional)**

    - Use `exec()` for scripts
    - Pass request headers via environment variables

14. **Logging**
    - Access logs in Apache/Nginx style
    - Error logs for malformed requests or crashes

---

## Phase 6 — Security & Robustness

15. **Input validation**

    - Prevent path traversal (`../etc/passwd`)
    - Enforce max request size
    - Drop suspicious headers

16. **Signal handling**

    - Handle `SIGINT` to gracefully close sockets
    - Reap zombie processes if using `fork()`

17. **Configuration system**
    - External config file for: port, document root, max clients, logging

---

## Phase 7 — Transition to HTTPS

18. **TLS integration with OpenSSL**

    - Initialize SSL context
    - Load certificate + private key
    - Wrap accepted socket with `SSL_accept()`
    - Replace `recv()/send()` with `SSL_read()/SSL_write()`

19. **Enforce HTTPS**

    - Redirect HTTP (port 80) to HTTPS (port 443)

20. **Cipher suites and hardening**
    - Disable SSLv2/SSLv3 and old TLS versions
    - Enable TLSv1.2/1.3

---

## Phase 8 — Production-Level Enhancements

21. **Request pipelining (optional)**

    - Handle multiple requests in one TCP stream

22. **File caching**

    - Cache popular files in memory

23. **Resource limits**

    - Max clients, max threads, request timeouts, rate limiting

24. **Basic authentication**

    - Parse `Authorization: Basic ...`
    - Restrict access to directories

25. **Extensive testing**
    - Test with `curl`, `ab`, `wrk`
    - Fuzz malformed requests for robustness
