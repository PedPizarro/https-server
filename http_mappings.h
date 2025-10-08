// http_mappings.h
#ifndef HTTP_MAPPINGS_H
#define HTTP_MAPPINGS_H

typedef struct
{
    const char *name;
    int value_case_insensitive; // 1 for case-insensitive value
    int trim_ows;               // 1 to trim OWS (optional whitespace) around delimiters
} header_mapping_t;

int is_method_allowed(const char *method);
int is_header_value_case_insensitive(const char *header_name);
int should_trim_ows(const char *header_name);
void normalize_header_name(char *name);
void normalize_header_value(char *header_line);
void normalize_header(char *header_line);
void normalize_string_lower(char *str);

#endif // HTTP_MAPPINGS_H
