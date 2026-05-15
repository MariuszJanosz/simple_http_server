#ifndef HTTP_FIELD_LINE_H
#define HTTP_FIELD_LINE_H

#include <stddef.h>

typedef enum Bucket_status_t {
    EMPTY = 0,
    OCCUPIED
} Bucket_status_t;

typedef struct Field_line_t {
    char* field_name;
    char* field_value;
} Field_line_t;

typedef struct Field_line_hash_map_bucket_t {
    Field_line_t field_line;
    Bucket_status_t bucket_status;
} Field_line_hash_map_bucket_t;

typedef struct Field_line_hash_map_t {
    Field_line_hash_map_bucket_t* buckets;
    size_t capacity;
    size_t occupied_count;
} Field_line_hash_map_t;

void init_field_line_hash_map(Field_line_hash_map_t* hm, const size_t capacity);
void free_field_line_hash_map(Field_line_hash_map_t* hm);
Field_line_t* find_field_line_in_hash_map(const Field_line_hash_map_t* hm, const char* field_name);
void add_field_line_to_hash_map(Field_line_hash_map_t* hm, const Field_line_t field_line);

#endif //HTTP_FIELD_LINE_H

