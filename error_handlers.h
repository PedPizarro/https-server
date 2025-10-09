#ifndef ERROR_HANDLERS_H
#define ERROR_HANDLERS_H

#include "http_errors.h"

void send_error_response(int client_fd, int status_code, const char *status_text, const char *connection_header, const char *method);

// Helper for 405 and other cases needing extra headers (e.g., Allow:)
void send_error_response_with_headers(int client_fd, int status_code, const char *status_text,
                                      const char *connection_header, const char *extra_headers, const char *method);

// Phase-specific status mappers. Returns 1 to continue, 0 if it handled the error
// and responded (or decided to close silently).
int handle_read_headers_status(int rc, int client_fd, const char *method);
int handle_request_line_status(int rc, int client_fd, const char *method);
int handle_parse_headers_status(int rc, int client_fd, const char *method);
int handle_validate_status(int rc, int client_fd, const char *method);
int handle_read_body_status(int rc, int client_fd, const char *connection_header, const char *method);

#endif
