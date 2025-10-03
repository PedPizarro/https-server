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
   - Set `SO_REUSEADDR` socket option to avoid "address already in use" errors during restarts

2. **Loop for multiple sequential clients**
   - Wrap `accept()` in a loop so the server handles one connection, closes it, then waits for another.

---

## Phase 2 — Minimal HTTP Request Handling

3. **Implement request parsing (basic)**

   - Read request line (e.g., `GET /index.html HTTP/1.1`)
   - Extract: method, path, version
   - Parse query string (e.g., split `/path?key=value` at `?` and store the query string in a char array)
   - Use fixed-size buffer (e.g., 1024 bytes) for path and validate length (`strlen(path) < MAX_PATH`) to prevent overflows
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
   - Set basic read timeout for `recv()` using `setsockopt()` with `SO_RCVTIMEO` (e.g., 5 seconds) to avoid hanging on slow clients

7. **Support for different methods**

   - GET (mandatory)
   - HEAD (headers only, no body)
   - POST (accept request body, echo/store it for now)

8. **HTTP/1.1 keep-alive support**
   - Parse `Connection:` header
   - If `keep-alive`, reuse socket for multiple requests
   - Implement timeout for idle connections

---

## Phase 4 — Networking and Request Enhancements

9. **Networking Enhancements**

   - Add IPv6 support for dual-stack operation (IPv4 and IPv6)
   - Configure connection backlog (`SOMAXCONN`) for high connection volumes
   - Handle network errors robustly (e.g., `ECONNRESET`, `EAGAIN`) across all socket operations
   - Implement minimal virtual hosting: Parse `Host` header to route requests to different document roots (e.g., `./www/domain1`, `./www/domain2`) via simple string comparison

10. **Request Parsing Improvements**

    - Implement simple header limits: Cap total header size (e.g., 8KB) and maximum number of headers (e.g., 50) to prevent abuse
    - Support comprehensive status codes (e.g., `301 Moved Permanently`, `429 Too Many Requests`) for all applicable scenarios

11. **Enhanced HTTP Method Implementations**
    - Implement and improve all HTTP/1.1 methods to align with standards (RFC 7230–7235)
    - **GET and HEAD**:
      - Support conditional requests (`If-Modified-Since`, `If-None-Match` with ETags) per RFC 7232
      - Implement range requests (`Range` header, `206 Partial Content`) per RFC 7233
      - Add caching headers (`Cache-Control`, `Last-Modified`, `ETag`) per RFC 7234
    - **POST**:
      - Add support for `multipart/form-data` to handle standard file uploads (e.g., images, documents) per RFC 7578, including parsing boundaries and form fields
      - Support `application/json` for structured data per RFC 8259, with JSON parsing and validation
      - Handle chunked transfer encoding (`Transfer-Encoding: chunked`) for large request bodies
    - **PUT**:
      - Implement resource creation or update at the specified URI (e.g., overwrite or create files in `./www`)
      - Return `201 Created` or `204 No Content` as appropriate per RFC 7231
    - **DELETE**:
      - Implement resource deletion at the specified URI (e.g., remove files from `./www`)
      - Return `200 OK` or `204 No Content` per RFC 7231
    - **OPTIONS**:
      - Implement support for CORS preflight requests, returning allowed methods and headers (e.g., `Allow`, `Access-Control-Allow-*`) per RFC 7231
      - Enable cross-origin resource sharing for web applications
    - **TRACE**:
      - Implement minimal support to echo requests for debugging, or disable with `405 Method Not Allowed` for security (e.g., prevent cross-site tracing)
    - **CONNECT**:
      - Implement basic support for HTTPS tunneling or reject with `405 Method Not Allowed` if proxying is not required
    - Ensure all methods support request pipelining (RFC 7230), handle chunked transfer encoding, and include robust error handling (e.g., `400 Bad Request`, `405 Method Not Allowed`)
    - Support custom methods (e.g., WebDAV's PROPFIND, PATCH) for extensibility

---

## Phase 5 — Advanced Features

12. **MIME type handling**

    - Lookup table for common extensions (html, css, js, jpg, png, etc.)
    - Return correct `Content-Type` header

13. **Directory listing (optional)**

    - If directory requested and no `index.html` exists, list files

14. **CGI or dynamic content (optional)**

    - Use `exec()` for scripts
    - Pass request headers via environment variables

15. **Logging**

    - Access logs in Apache/Nginx style
    - Error logs for malformed requests or crashes

16. **Unit Testing**
    - Implement automated unit tests for core components (e.g., request parser, header handling) using a C testing framework like Check or CMockery

---

## Phase 6 — Debugging Tools

17. **Debugging and Profiling**
    - Use tools like Valgrind for memory leak detection and GDB for step-through debugging
    - Implement basic logging of request/response cycles to a file for troubleshooting

---

## Phase 7 — Security & Robustness

18. **Input validation**

    - Prevent path traversal (`../etc/passwd`)
    - Enforce max request size
    - Drop suspicious headers
    - Sanitize input headers to reject malformed or overly long headers
    - Implement rate limiting (e.g., per IP) to prevent abuse and DDoS attacks
    - Use explicit buffer protections (e.g., `strncpy` instead of `strcpy`, bounds checking for all string operations)
    - Support secure cookie handling (`Set-Cookie` with `Secure`, `HttpOnly`, `SameSite` attributes)
    - Add CSRF protection for POST, PUT, and DELETE methods

19. **Signal handling**

    - Handle `SIGINT` to gracefully close sockets
    - Reap zombie processes if using `fork()`

20. **Configuration system**
    - External config file for: port, document root, max clients, logging
    - Support configurable error pages and custom server behaviors

---

## Phase 8 — Transition to HTTPS

21. **TLS integration with OpenSSL**

    - Initialize SSL context
    - Load certificate + private key
    - Generate self-signed certificates for testing using OpenSSL
    - Wrap accepted socket with `SSL_accept()`
    - Replace `recv()/send()` with `SSL_read()/SSL_write()`
    - Handle TLS session resumption for performance using `SSL_SESSION`

22. **Enforce HTTPS**

    - Redirect HTTP (port 80) to HTTPS (port 443)

23. **Cipher suites and hardening**
    - Disable SSLv2/SSLv3 and old TLS versions
    - Enable TLSv1.2/1.3
    - Implement OCSP stapling and certificate revocation checking
    - Add security headers (e.g., HSTS, Content-Security-Policy, X-Frame-Options)

---

## Phase 9 — Concurrency & Scalability

24. **Basic Concurrency**

    - Support concurrent clients using:
      - Option A: `fork()` per client
      - Option B: threads with `pthread_create()`
      - Option C: non-blocking I/O with `select()`, `poll()`, or `epoll()`
    - Ensure thread safety for logging and shared resources using mutexes or semaphores
    - Handle `SIGCHLD` to prevent zombie processes with `fork()`
    - Handle race conditions in concurrent file access

25. **Advanced Concurrency Optimizations**
    - Optimize event-driven I/O with advanced mechanisms (e.g., `io_uring` on Linux, kqueue on BSD)
    - Implement thread or process pooling to reduce overhead compared to per-request `fork()` or `pthread_create()`
    - Add non-blocking socket configuration (`fcntl(O_NONBLOCK)`) for scalable I/O

---

## Phase 10 — Production-Level Enhancements

26. **Request pipelining (optional)**

    - Handle multiple requests in one TCP stream

27. **File caching**

    - Cache popular files in memory
    - Use `sendfile()` or zero-copy techniques for efficient static file serving

28. **Resource limits**

    - Max clients, max threads, request timeouts

29. **Basic authentication**

    - Parse `Authorization: Basic ...`
    - Restrict access to directories

30. **WebSocket Support (optional)**

    - Implement WebSocket protocol (RFC 6455) for real-time applications via HTTP/1.1 upgrade

31. **Connection Pooling (if using CGI/dynamic content)**

    - Pool connections to databases or external services to avoid resource exhaustion

32. **Testing and Validation**

    - Test with `curl`, `ab`, `wrk`
    - Fuzz malformed requests for robustness
    - Implement unit and integration tests for individual components and end-to-end flows
    - Expand fuzzing to cover headers, request bodies, and edge cases

33. **Deployment and Operations**

    - Containerization (e.g., Docker) for consistent deployment environments
    - Orchestration (e.g., Kubernetes) for scaling and high availability
    - CI/CD pipelines for automated testing, building, and deployment

34. **Monitoring and Metrics**

    - Integrate metrics collection (e.g., Prometheus) for request rates, error rates, and latency
    - Centralized logging aggregation (e.g., ELK stack) for analysis and debugging
    - Implement health checks and alerting for server uptime and issues

35. **Compliance and Auditing**

    - Perform automated security scans (e.g., OWASP ZAP) to identify vulnerabilities
    - Ensure compliance with standards like GDPR and PCI-DSS
    - Conduct load and stress testing (e.g., Locust) beyond basic tools

36. **Configuration and Extensibility**

    - Develop a modular architecture (e.g., plugin system for custom request handlers or middleware)
    - Add inline code documentation (e.g., Doxygen comments) for maintainability

37. **Performance Optimizations**
    - Optimize memory usage with memory pools and bounds checking to prevent leaks or overflows

---

## Phase 11 — Maintenance and Optimization

38. **Graceful Operations**

    - Support zero-downtime restarts for updates without service interruption
    - Enable dynamic configuration reloading without restarting the server

39. **Compression**

    - Implement gzip and Brotli compression for HTTP responses to reduce bandwidth

40. **Backup and Recovery**

    - Implement logging backup strategies to prevent data loss
    - Support server state recovery after crashes or restarts

41. **Versioning and Documentation**

    - Maintain a changelog for tracking releases and changes
    - Implement versioning for production deployments and API compatibility

42. **HTTP/2 Support**
    - Support HTTP/2 for request multiplexing and performance improvements
