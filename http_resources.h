#ifndef HTTP_RESOURCES_H
#define HTTP_RESOURCES_H

#include "status.h"

#include <stddef.h>

#include <sys/types.h>

typedef struct Field_line_config_t {
    char* field_name;
    char* func;
    char* field_value;
} Field_line_config_t;

typedef struct Resource_t {
    char* target;
    Http_status_t status;
    Field_line_config_t* field_line_configs;
    size_t count;
    size_t capacity;
} Resource_t;

typedef struct Bucket_t {
    char* path;
    size_t resource_index;
    int is_present;
} Bucket_t;

typedef struct Path_hash_map_t {
    Bucket_t* buckets;
    size_t capacity;
    size_t count;
} Path_hash_map_t;

extern Path_hash_map_t g_path_to_resource_index_hm;
extern Resource_t* g_resources_array;
extern size_t g_resources_count;

void init_resources_from_config();
ssize_t resource_index_for_path(const char* path);

#endif //HTTP_RESOURCES_H

