#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "http_errors.h"
#include "error_handlers.h"
#include "response_utils.h"
#include "http_mappings.h"

// Send error response
void send_error_response(int client_fd, int status_code, const char *status_text, const char *connection_header, const char *method)
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
                       "Connection: %s\r\n",
                       (method && str_case_cmp(method, "HEAD") == 0)
                           ? 0
                           : (int)strlen(body),
                       connection_header);

    if (offset >= sizeof(headers))
    {
        fprintf(stderr, "Error: Headers buffer too small\n");
        return;
    }

    // Combine headers and body, if not HEAD
    if (!method || str_case_cmp(method, "HEAD") != 0)
    {
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
    }

    if (send(client_fd, headers, offset, 0) < 0)
    {
        perror("send failed");
    }
    printf("Sent %d %s response\n", status_code, status_text);
}

// Minimal helper that can attach extra headers (e.g., Allow:)
void send_error_response_with_headers(int client_fd, int status_code, const char *status_text, const char *connection_header, const char *extra_headers, const char *method)
{
    char headers[1536] = {0};
    size_t offset = 0;
    char body[256];

    // Body
    snprintf(body, sizeof(body),
             "<html><body><h1>%d %s</h1></body></html>",
             status_code, status_text);

    // Status line
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "HTTP/1.1 %d %s\r\n", status_code, status_text);

    add_date_header(headers, &offset, sizeof(headers));

    // Extra headers (e.g., Allow:)
    if (extra_headers && *extra_headers)
    {
        offset += snprintf(headers + offset, sizeof(headers) - offset, "%s", extra_headers);
    }

    // Standard headers
    offset += snprintf(headers + offset, sizeof(headers) - offset,
                       "Content-Type: text/html\r\n"
                       "Content-Length: %d\r\n"
                       "Connection: %s\r\n",
                       (method && str_case_cmp(method, "HEAD") == 0)
                           ? 0
                           : (int)strlen(body),
                       connection_header);

    if (offset >= sizeof(headers))
        return;

    // Combine headers and body, if not HEAD
    if (!method || str_case_cmp(method, "HEAD") != 0)
    {
        size_t body_len = strlen(body);
        if (offset + body_len < sizeof(headers))
        {
            memcpy(headers + offset, body, body_len);
            offset += body_len;
        }
        else
        {
            return;
        }
    }

    if (send(client_fd, headers, offset, 0) < 0)
    {
        perror("send failed");
    }
    printf("Sent %d %s response\n", status_code, status_text);
}

// ----- Phase-specific helpers -----

int handle_read_headers_status(int rc, int client_fd, const char *method)
{
    if (rc > 0)
        return 1;

    switch (rc)
    {
    case HTTP_IO_EOF:
        printf("Client closed connection\n");
        return 0;
    case HTTP_IO_TIMEOUT:
        printf("Keep-alive timeout expired\n");
        return 0; // quiet close (no 408)
    case HTTP_IO_TIMEOUT_PARTIAL:
        send_error_response(client_fd, 408, "Request Timeout", "close", method);
        return 0;
    case HTTP_IO_EOF_PARTIAL:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    case HTTP_HEADERS_TOO_LARGE:
        send_error_response(client_fd, 431, "Request Header Fields Too Large", "close", method);
        return 0;
    case HTTP_PARSE_ERROR:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    case HTTP_IO_ERROR:
    default:
        // real I/O error or unknown: just close
        return 0;
    }
}

int handle_request_line_status(int rc, int client_fd, const char *method)
{
    if (rc > 0)
        return 1;

    switch (rc)
    {
    case HTTP_URI_TOO_LONG:
        send_error_response(client_fd, 414, "URI Too Long", "close", method);
        return 0;
    case HTTP_VERSION_UNSUPPORTED:
        send_error_response(client_fd, 505, "HTTP Version Not Supported", "close", method);
        return 0;
    case HTTP_METHOD_NOT_ALLOWED:
        send_error_response_with_headers(client_fd, 405, "Method Not Allowed", "close", build_allow_header(), method);
        return 0;
    case HTTP_NOT_IMPLEMENTED:
        send_error_response(client_fd, 501, "Not Implemented", "close", method);
        return 0;
    case HTTP_PARSE_ERROR:
    default:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    }
}

int handle_parse_headers_status(int rc, int client_fd, const char *method)
{
    if (rc > 0)
        return 1;

    switch (rc)
    {
    case HTTP_HEADERS_TOO_LARGE:
        send_error_response(client_fd, 431, "Request Header Fields Too Large", "close", method);
        return 0;
    case HTTP_PARSE_ERROR:
    default:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    }
}

int handle_validate_status(int rc, int client_fd, const char *method)
{
    if (rc > 0)
        return 1;

    switch (rc)
    {
    case HTTP_LENGTH_REQUIRED:
        send_error_response(client_fd, 411, "Length Required", "close", method);
        return 0;
    case HTTP_NOT_IMPLEMENTED:
        send_error_response(client_fd, 501, "Not Implemented", "close", method);
        return 0;
    case HTTP_METHOD_NOT_ALLOWED:
        send_error_response_with_headers(client_fd, 405, "Method Not Allowed", "close", build_allow_header(), method);
        return 0;
    case HTTP_PARSE_ERROR:
    default:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    }
}

int handle_read_body_status(int rc, int client_fd, const char *connection_header, const char *method)
{
    if (rc >= 0)
        return 1; // 0 == success/no-body

    switch (rc)
    {
    case HTTP_BODY_TOO_LARGE:
        send_error_response(client_fd, 413, "Payload Too Large",
                            connection_header ? connection_header : "close", method);
        return 0;
    case HTTP_IO_TIMEOUT_PARTIAL:
        send_error_response(client_fd, 408, "Request Timeout", "close", method);
        return 0;
    case HTTP_IO_EOF_PARTIAL:
        send_error_response(client_fd, 400, "Bad Request", "close", method);
        return 0;
    case HTTP_IO_ERROR:
    default:
        return 0; // silent close
    }
}
