#ifndef HTTP_COMMON_FIELD_LINES_H
#define HTTP_COMMON_FIELD_LINES_H

#include <stddef.h>

typedef enum Field_line_type_t {
    LIST,
    LIST_NONEMPTY,
    SINGLETON,
    SINGLETON_WITH_DEDUPLICATION,
    MULTILINE_DO_NOT_MERGE,
    UNKNOWN_TYPE
} Field_line_type_t;

typedef struct Field_line_attribute_t {
    const char* field_name;
    Field_line_type_t type;
} Field_line_attributes_t;

extern const Field_line_attributes_t* g_common_field_lines_attributes;
extern const size_t g_common_field_lines_count;

const Field_line_attributes_t* find_field_line_attributes(const char* field_name);

#endif //HTTP_COMMON_FIELD_LINES_H

