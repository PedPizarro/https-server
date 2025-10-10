#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>

// Project headers
#include "http_mappings.h"
#include "string_utils.h"
#include "http_errors.h"
#include "error_handlers.h"
#include "response_utils.h"

#define PORT 8080
#define MAX_REQUEST_SIZE 65536 // 64KB max request
#define MAX_HEADERS 100
#define MAX_PATH 2048        // 2KB max path length
#define MAX_QUERY 1024       // 1KB max query length
#define MAX_HEADER_LINE 8192 // 8KB max header line
#define MAX_METHOD 16
#define MAX_VERSION 16

#define READ_TIMEOUT_SEC 30      // 30 second timeout
#define KEEP_ALIVE_TIMEOUT_SEC 5 // 5 second keep-alive timeout

typedef struct
{
    char method[MAX_METHOD];
    char path[MAX_PATH];
    char query[MAX_QUERY];
    char version[MAX_VERSION];
    char headers[MAX_HEADERS][MAX_HEADER_LINE];
    int header_count;
    char *body;
    size_t body_length;
    size_t content_length;
    char connection_header[32];
} http_request;

typedef struct
{
    const char *ext;
    const char *type;
} mime_type;

static const mime_type mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".txt", "text/plain"},
    {NULL, "application/octet-stream"}};

const char *get_mime_type(const char *filepath)
{
    const char *ext = strrchr(filepath, '.');
    if (!ext)
        return mime_types[sizeof(mime_types) / sizeof(mime_type) - 1].type;
    for (size_t i = 0; mime_types[i].ext; i++)
    {
        if (str_case_cmp(ext, mime_types[i].ext) == 0)
        {
            return mime_types[i].type;
        }
    }
    return mime_types[sizeof(mime_types) / sizeof(mime_type) - 1].type;
}

// Set socket timeout
int set_socket_timeout(int sockfd, int seconds)
{
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        return -1;
    }
    return 0;
}

// Read data with proper error handling and timeouts
ssize_t read_with_timeout(int sockfd, char *buffer, size_t size)
{
    ssize_t bytes_read = recv(sockfd, buffer, size, 0);

    if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            printf("Socket timeout occurred\n");
            return HTTP_IO_TIMEOUT;
        }
        else
        {
            perror("recv() failed");
            return HTTP_IO_ERROR;
        }
    }

    return bytes_read;
}

// Read complete HTTP headers (until \r\n\r\n)
int read_http_headers(int client_fd, char *buffer, size_t buffer_size)
{
    size_t total_read = 0;
    char *header_end = NULL;

    printf("Reading HTTP headers...\n");

    // Keep reading until we find \r\n\r\n or hit limits
    while (total_read < buffer_size - 1 && !header_end)
    {
        // Read more data
        ssize_t bytes = read_with_timeout(client_fd,
                                          buffer + total_read,
                                          buffer_size - total_read - 1);

        if (bytes == HTTP_IO_TIMEOUT)
        {
            // Timeout: idle vs partial
            return (total_read == 0) ? HTTP_IO_TIMEOUT : HTTP_IO_TIMEOUT_PARTIAL;
        }
        if (bytes == HTTP_IO_ERROR)
        {
            return HTTP_IO_ERROR;
        }
            if (bytes == 0)
            {
            // Client close: idle vs partial
            return (total_read == 0) ? HTTP_IO_EOF : HTTP_IO_EOF_PARTIAL;
            }
        if (bytes < 0)
        {
            // Propagate other negative enums (future-proofing, shouldn't happen here)
            return (int)bytes;
        }

        total_read += bytes;
        buffer[total_read] = '\0';

        printf("Read %zd bytes (total: %zu)\n", bytes, total_read);

        // Check if we have complete headers
        header_end = strstr(buffer, "\r\n\r\n");

        if (header_end)
        {
            printf("Found complete headers (end at position %ld)\n",
                   header_end - buffer);
            break;
        }

        // Prevent infinite reading
        if (total_read > MAX_REQUEST_SIZE / 2)
        {
            printf("Headers too large (%zu bytes)\n", total_read);
            return HTTP_HEADERS_TOO_LARGE;
        }
    }

    if (!header_end)
    {
        printf("Headers incomplete\n");
        return HTTP_PARSE_ERROR;
    }

    return total_read;
}

// Extract Content-Length from headers
size_t get_content_length(const http_request *req)
{
    for (int i = 0; i < req->header_count; i++)
    {
        if (strn_case_cmp(req->headers[i], "Content-Length:", 15) == 0)
        {
            const char *value = req->headers[i] + 15;
            // Skip whitespace
            while (*value == ' ' || *value == '\t')
                value++;

            unsigned long length = strtoul(value, NULL, 10);
            if (length == 0 && errno == EINVAL)
            {
                return 0; // Invalid number
            }
            return (size_t)length;
        }
    }
    return 0; // No Content-Length header
}

// Read HTTP request body based on Content-Length
int read_http_body(int client_fd, char *buffer, size_t headers_end_pos,
                   size_t total_read, http_request *req)
{

    req->content_length = get_content_length(req);

    if (req->content_length == 0)
    {
        req->body = NULL;
        req->body_length = 0;
        return 0; // No body to read
    }

    printf("Content-Length: %zu bytes\n", req->content_length);

    // Validate content length
    if (req->content_length > MAX_REQUEST_SIZE)
    {
        printf("Content-Length too large: %zu bytes for %d\n", req->content_length, MAX_REQUEST_SIZE);
        return HTTP_BODY_TOO_LARGE;
    }

    // Calculate how much body data we already have
    size_t headers_length = headers_end_pos + 4; // +4 for \r\n\r\n
    size_t body_already_read = (total_read > headers_length) ? (total_read - headers_length) : 0;

    printf("Already have %zu body bytes, need %zu more\n",
           body_already_read, req->content_length - body_already_read);

    // Set body pointer to start of body data in buffer
    req->body = buffer + headers_length;

    // Read remaining body data
    while (body_already_read < req->content_length)
    {
        size_t bytes_needed = req->content_length - body_already_read;
        size_t buffer_space = MAX_REQUEST_SIZE - (headers_length + body_already_read);
        size_t to_read = (bytes_needed < buffer_space) ? bytes_needed : buffer_space;

        if (to_read == 0)
        {
            printf("Request too large for buffer\n");
            return HTTP_BODY_TOO_LARGE;
        }

        ssize_t bytes = read_with_timeout(client_fd,
                                          buffer + headers_length + body_already_read,
                                          to_read);

        if (bytes == HTTP_IO_TIMEOUT)
        {
            printf("Timeout mid-body\n");
            return HTTP_IO_TIMEOUT_PARTIAL;
        }
        if (bytes == 0)
        {
            printf("EOF mid-body\n");
            return HTTP_IO_EOF_PARTIAL;
        }
        if (bytes < 0)
        {
            return bytes;
        }

        body_already_read += bytes;
        printf("Read %zd body bytes (%zu/%zu complete)\n",
               bytes, body_already_read, req->content_length);
    }

    req->body_length = req->content_length;
    return 0;
}

// Parse request line with validation
int parse_request_line(const char *line, http_request *req)
{
    char method[MAX_METHOD];
    char full_path[MAX_PATH];
    char version[MAX_VERSION];

    // Parse with size limits
    if (sscanf(line, "%15s %2047s %15s", method, full_path, version) != 3)
    {
        printf("Failed to parse request line: '%s'\n", line);
        return HTTP_PARSE_ERROR;
    }

    size_t path_len = strlen(full_path);

    // Split path and query
    char *query_start = strchr(full_path, '?');
    if (query_start)
    {
        path_len = query_start - full_path;
        if (path_len >= MAX_PATH || strlen(query_start + 1) >= MAX_QUERY)
        {
            printf("Path or query too long\n");
            return HTTP_URI_TOO_LONG;
        }
        req->path[path_len] = '\0';
        strcpy(req->query, query_start + 1);
    }
    else
    {
        if (path_len >= MAX_PATH)
        {
            printf("Path too long\n");
            return HTTP_URI_TOO_LONG;
        }
        req->query[0] = '\0';
    }

    // Validate lengths
    if (strlen(method) >= MAX_METHOD ||
        strlen(version) >= MAX_VERSION)
    {
        printf("Request line components too long\n");
        return HTTP_PARSE_ERROR;
    }

    // Accept only known methods
    if (!is_method_allowed(method))
    {
        printf("Unsupported method: %s\n", method);
        return HTTP_METHOD_NOT_ALLOWED;
    }

    // Validate method characters (only uppercase letters)
    for (char *p = method; *p; p++)
    {
        if (*p < 'A' || *p > 'Z')
        {
            printf("Invalid method: %s\n", method);
            return HTTP_PARSE_ERROR;
        }
    }

    // Validate version format
    if (strncmp(version, "HTTP/", 5) != 0)
    {
        printf("Invalid HTTP version: %s\n", version);
        return 0;
    }

    strcpy(req->method, method);
    strcpy(req->version, version);
    strncpy(req->path, full_path, path_len);
    req->path[path_len] = '\0';

    return 1;
}

// Parse headers with proper validation
int parse_headers(const char *request_data, http_request *req)
{
    const char *line_start = strstr(request_data, "\r\n");
    if (!line_start)
        return HTTP_PARSE_ERROR;

    line_start += 2; // Skip first \r\n
    req->header_count = 0;

    while (req->header_count < MAX_HEADERS && line_start && *line_start != '\r')
    {
        const char *line_end = strstr(line_start, "\r\n");
        if (!line_end)
            break;

        size_t line_len = line_end - line_start;
        if (line_len == 0)
            break; // Empty line = end of headers

        if (line_len >= MAX_HEADER_LINE)
        {
            printf("Header line too long (%zu bytes)\n", line_len);
            return HTTP_HEADERS_TOO_LARGE;
        }

        // Validate header format (must contain :)
        const char *colon = memchr(line_start, ':', line_len);
        if (!colon)
        {
            printf("Invalid header format (no colon)\n");
            return HTTP_PARSE_ERROR;
        }

        // Copy header
        strncpy(req->headers[req->header_count], line_start, line_len);
        req->headers[req->header_count][line_len] = '\0';

        // Normalize header name and value to lowercase
        normalize_header_name(req->headers[req->header_count]);
        normalize_header_value(req->headers[req->header_count]);

        // Check for Connection header
        if (strn_case_cmp(req->headers[req->header_count], "Connection:", 11) == 0)
        {
            const char *value = req->headers[req->header_count] + 11;
            size_t value_len = strlen(value);

            if (value_len < sizeof(req->connection_header))
                strcpy(req->connection_header, value);
        }

        req->header_count++;

        // Jump over \r\n to go to next line
        line_start = line_end + 2;
    }

    printf("Parsed %d headers\n", req->header_count);
    for (int i = 0; i < req->header_count; i++)
    {
        printf("Header[%d]: %s\n", i, req->headers[i]);
    }

    return 1;
}

// Check for required headers
int validate_http_request(const http_request *req)
{
    // HTTP/1.1 requires Host header
    if (strcmp(req->version, "HTTP/1.1") == 0)
    {
        int has_host = 0;
        for (int i = 0; i < req->header_count; i++)
        {
            if (strn_case_cmp(req->headers[i], "Host:", 5) == 0)
            {
                has_host = 1;
                break;
            }
        }
        if (!has_host)
        {
            printf("HTTP/1.1 request missing Host header\n");
            return 0;
        }
    }

    return 1;
}

// Path traversal protection
int is_safe_path(const char *path)
{
    if (strstr(path, "..") != NULL)
    {
        printf("Path traversal attempt: %s\n", path);
        return 0;
    }

    if (path[0] != '/')
    {
        printf("Path must start with /: %s\n", path);
        return 0;
    }

    return 1;
}

int add_date_header(char *buf, size_t *offset, size_t max_len)
{
    time_t now;
    struct tm *gm;
    if (time(&now) == -1 || (gm = gmtime(&now)) == NULL)
    {
        fprintf(stderr, "Failed to get time for Date header\n");
        if (*offset < max_len)
        {
            *offset += snprintf(buf + *offset, max_len - *offset, "Date: Error\r\n");
            return -1;
        }
        return -1;
    }
    size_t len = strftime(buf + *offset, max_len - *offset, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", gm);
    if (len == 0)
    {
        fprintf(stderr, "Date header buffer too small\n");
        return -1;
    }
    *offset += len;
    return 0;
}

// Send error response
void send_error_response(int client_fd, int status_code, const char *status_text, const char *connection_header)
{
    char headers[1024] = {0};
    size_t offset = 0;
    char body[256];

    snprintf(body, sizeof(body),
             "<html><body><h1>%d %s</h1></body></html>",
             status_code, status_text);

    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "HTTP/1.1 %d %s\r\n", status_code, status_text);
    add_date_header(headers, &offset, sizeof(headers));
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "Content-Type: text/html\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: %s\r\n"
                       "\r\n",
                       (int)strlen(body), connection_header);

    if (offset >= sizeof(headers))
    {
        fprintf(stderr, "Error: Headers buffer too small\n");
        return;
    }

    // Combine headers and body
    size_t body_len = strlen(body);
    if (offset + body_len < sizeof(headers))
    {
        memcpy(headers + offset, body, body_len);
        offset += body_len;
    }
    else
    {
        fprintf(stderr, "Error: Headers+body buffer too small\n");
        return;
    }

    if (send(client_fd, headers, offset, 0) < 0)
    {
        perror("send failed");
    }
    printf("Sent %d %s response\n", status_code, status_text);
}

// Send file response
void send_file_response(int client_fd, const char *filepath, const char *method, const char *connection_header)
{
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        send_error_response(client_fd, 404, "Not Found", connection_header, method);
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0)
    {
        fclose(file);
        send_error_response(client_fd, 500, "Internal Server Error", connection_header, method);
        return;
    }

    // Build headers
    char headers[1024] = {0};
    size_t offset = 0;
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "HTTP/1.1 200 OK\r\n");
    add_date_header(headers, &offset, sizeof(headers));
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "Content-Type: %s\r\n"
                       "Content-Length: %ld\r\n"
                       "Connection: %s\r\n"
                       "\r\n",
                       get_mime_type(filepath), file_size, connection_header);

    if (offset >= sizeof(headers))
    {
        fclose(file);
        fprintf(stderr, "Error: Headers buffer too small\n");
        send_error_response(client_fd, 500, "Internal Server Error", connection_header, method);
        return;
    }

    // Send headers
    if (send(client_fd, headers, offset, 0) < 0)
    {
        fclose(file);
        perror("send failed");
        return;
    }

    // Send file content only if method is GET
    if (str_case_cmp(method, "GET") == 0)
    {
        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        {
            if (send(client_fd, buffer, bytes_read, 0) < 0)
            {
                perror("send failed");
                break;
            }
        }
    }

    fclose(file);
    printf("Sent file: %s (%ld bytes)\n", filepath, file_size);
}

// Map URL path to file path
int map_path_to_file(const char *url_path, char *file_path, size_t max_len)
{
    if (strcmp(url_path, "/") == 0)
    {
        snprintf(file_path, max_len, "./www/index.html");
    }
    else
    {
        int bytes = snprintf(file_path, max_len, "./www%s", url_path);
        if (bytes < 0 || (size_t)bytes >= max_len)
        {
            fprintf(stderr, "File path too long\n");
            return HTTP_URI_TOO_LONG;
        }
    }
    return 0;
}

void handle_post_request(int client_fd, const http_request *request, const char *connection_header)
{
    // Store body to file
    if (request->body_length > 0 && strcmp(request->path, "/test") == 0)
    {
        char dir_path[1024];
        if (map_path_to_file(request->path, dir_path, sizeof(dir_path)) != 0)
        {
            send_error_response(client_fd, 414, "URI Too Long", "close", request->method);
            return;
        }
        size_t dir_path_len = strlen(dir_path);

        struct stat st;
        if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "Directory %s does not exist or is not a directory\n", dir_path);
            send_error_response(client_fd, 500, "Internal Server Error", request->connection_header, request->method);
            return;
        }

        // Check Content-Type
        const char *content_type = NULL;
        for (int i = 0; i < request->header_count; i++)
        {
            if (strn_case_cmp(request->headers[i], "Content-Type:", 13) == 0)
            {
                content_type = request->headers[i] + 13;
                while (*content_type == ' ' || *content_type == '\t')
                    content_type++;
                break;
            }
        }

        char log_path[1024];
        FILE *log = NULL;

        if (content_type && strn_case_cmp(content_type, "image/", 6) == 0) // Handle image (binary) data
        {
            const char *subtype = content_type + 6;
            char extension[64] = "bin"; // Fallback extension

            // Extract subtype up to ';' (if optional parameters present) or end, and convert to lowercase
            size_t ext_len = 0;
            for (const char *p = subtype; *p && *p != ';' && ext_len < sizeof(extension) - 1; p++, ext_len++)
            {
                extension[ext_len] = tolower(*p);
            }
            extension[ext_len] = '\0';

            // Validate: only alphanumeric characters allowed
            for (size_t i = 0; extension[i]; i++)
            {
                if (!isalnum(extension[i]))
                {
                    strcpy(extension, "bin"); // Fallback for invalid subtypes
                    break;
                }
            }

            if (dir_path_len > sizeof(log_path) - strlen("/image.") - ext_len - 1)
            {
                fprintf(stderr, "Directory path too long for image log: %s\n", dir_path);
                send_error_response(client_fd, 500, "Internal Server Error", request->connection_header, request->method);
                return;
            }
            snprintf(log_path, sizeof(log_path), "%s/image.%s", dir_path, extension);

            // Open in binary mode
            log = fopen(log_path, "wb");
        }
        else if (content_type && // Handle text data
                 (strn_case_cmp(content_type, "text/", 5) == 0 ||
                  strn_case_cmp(content_type, "application/json", 16) == 0 ||
                  strn_case_cmp(content_type, "application/x-www-form-urlencoded", 33) == 0))
        {
            snprintf(log_path, sizeof(log_path), "%s/post.log", dir_path);

            // Open in text mode, append
            log = fopen(log_path, "a");
        }
        else
        {
            fprintf(stderr, "Unsupported Content-Type: %s\n", content_type ? content_type : "none");
            send_error_response(client_fd, 415, "Unsupported Media Type", request->connection_header, request->method);
            return;
        }

        if (!log)
        {
            fprintf(stderr, "Failed to open %s for writing: %s\n", log_path, strerror(errno));
            send_error_response(client_fd, 500, "Internal Server Error", request->connection_header, request->method);
            return;
        }

        fwrite(request->body, 1, request->body_length, log);

        // Append newline for text files only
        if (content_type && (strn_case_cmp(content_type, "text/", 5) == 0 ||
                             strn_case_cmp(content_type, "application/json", 16) == 0 ||
                             strn_case_cmp(content_type, "application/x-www-form-urlencoded", 33) == 0))
        {
            fwrite("\n", 1, 1, log);
        }
        fclose(log);
    }

    // Build response
    char headers[1024] = {0};
    size_t offset = 0;
    char response_body[1024];
    int body_len = 0;

    if (request->body_length > 0)
    {
        body_len = snprintf(response_body, sizeof(response_body), "Received: %.*s",
                            (int)request->body_length, request->body);
    }
    else
    {
        body_len = snprintf(response_body, sizeof(response_body), "Received empty POST request to %s",
                            request->path);
    }

    offset += snprintf(headers + offset, sizeof(headers) - offset, "HTTP/1.1 200 OK\r\n");
    add_date_header(headers, &offset, sizeof(headers));
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: %s\r\n"
                       "\r\n",
                       body_len, connection_header);

    if (offset >= sizeof(headers))
    {
        fprintf(stderr, "Error: Headers buffer too small\n");
        send_error_response(client_fd, 500, "Internal Server Error", connection_header, request->method);
        return;
    }

    // Combine headers and body
    if (offset + body_len < sizeof(headers))
    {
        memcpy(headers + offset, response_body, body_len);
        offset += body_len;
    }
    else
    {
        fprintf(stderr, "Error: Headers+body buffer too small\n");
        send_error_response(client_fd, 500, "Internal Server Error", connection_header, request->method);
        return;
    }

    if (send(client_fd, headers, offset, 0) < 0)
    {
        perror("send failed");
    }

    printf("Handled POST request to %s with %zu bytes\n", request->path, request->body_length);
}

// Main request handler with proper HTTP parsing
void handle_client(int client_fd)
{
    char *buffer = malloc(MAX_REQUEST_SIZE);
    if (!buffer)
    {
        printf("Failed to allocate request buffer\n");
        return;
    }

    // Set socket timeout
    set_socket_timeout(client_fd, READ_TIMEOUT_SEC);

    do
    {
        http_request request = {0};

        int error_code = 0;

        // Step 1: Read complete headers
        int total_read = read_http_headers(client_fd, buffer, MAX_REQUEST_SIZE);
        if (!handle_read_headers_status(total_read, client_fd, request.method))
        {
            break;
        }

        // Step 2: Find where headers end
        char *header_end = strstr(buffer, "\r\n\r\n");

        // Step 3: Parse request line
        char *first_line_end = strstr(buffer, "\r\n");

        *first_line_end = '\0'; // Temporarily null-terminate first line
        error_code = parse_request_line(buffer, &request);
        if (!handle_request_line_status(error_code, client_fd, request.method))
        {
            break;
        }
        *first_line_end = '\r'; // Restore buffer

        printf("Request: %s %s %s\n", request.method, request.path, request.version);

        // Step 4: Parse headers
        error_code = parse_headers(buffer, &request);
        if (!handle_parse_headers_status(error_code, client_fd, request.method))
        {
            break;
        }

        // Step 5: Validate request
        error_code = validate_http_request(&request);
        if (!handle_validate_status(error_code, client_fd, request.method))
        {
            break;
        }

        // Step 6: Read body if present (for POST/PUT requests)
        error_code = read_http_body(client_fd, buffer, (size_t)(header_end - buffer), (size_t)total_read, &request);
        if (!handle_read_body_status(error_code, client_fd, request.connection_header, request.method))
        {
            break;
        }

        if (request.body_length > 0)
        {
            printf("Request body: %zu bytes\n", request.body_length);
            // For debugging, print first 100 chars of body
            char preview[101];
            size_t preview_len = (request.body_length < 100) ? request.body_length : 100;
            strncpy(preview, request.body, preview_len);
            preview[preview_len] = '\0';
            printf("Body preview: %.100s%s\n", preview,
                   (request.body_length > 100) ? "..." : "");
        }

        // Step 7: Validate path safety
        if (!is_safe_path(request.path))
        {
            send_error_response(client_fd, 400, "Bad Request", request.connection_header, request.method);
            break;
        }

        // Step 8: Handle different methods
        if (strcmp(request.method, "GET") == 0 || strcmp(request.method, "HEAD") == 0)
        {
            char file_path[1024];
            if (map_path_to_file(request.path, file_path, sizeof(file_path)) != 0)
            {
                send_error_response(client_fd, 414, "URI Too Long", "close", request.method);
                break;
            }
            send_file_response(client_fd, file_path, request.method, request.connection_header);
        }
        else if (strcmp(request.method, "POST") == 0)
        {
            handle_post_request(client_fd, &request, request.connection_header);
        }
        else
        {
            send_error_response_with_headers(client_fd, 405, "Method Not Allowed", request.connection_header, build_allow_header(), request.method);
        }

        printf("connection header: %s\n", request.connection_header);
        if (strn_case_cmp(request.connection_header, "keep-alive", 10) != 0)
        {
            break;
        }

        set_socket_timeout(client_fd, KEEP_ALIVE_TIMEOUT_SEC);
    } while (1);

    free(buffer);
}

// Main function
int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    printf("Starting HTTP server on port %d...\n", PORT);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket() failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt() failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind() failed");
        exit(1);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen() failed");
        exit(1);
    }

    printf("Server listening on http://localhost:%d\n", PORT);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept() failed");
            continue;
        }

        printf("\n=== New connection from %s:%d ===\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        handle_client(client_fd);
        close(client_fd);
        printf("=== Connection closed ===\n");
    }

    close(server_fd);
    return 0;
}