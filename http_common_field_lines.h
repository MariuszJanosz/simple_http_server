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

extern const char** common_field_line_names;
extern const Field_line_type_t* common_field_line_types;
extern const size_t common_field_lines_count;

#endif //HTTP_COMMON_FIELD_LINES_H

