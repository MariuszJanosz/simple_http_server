#ifndef HTTP_RESOURCES_H
#define HTTP_RESOURCES_H

#include "status.h"

#include <stddef.h>

typedef struct Field_line_config_t {
    char* field_name;
    void* func;
    char* field_value;
} Field_line_config_t;

typedef struct Resource_t {
    char* target;
    Http_status_t status;
    Field_line_config_t* field_line_configs;
    size_t count;
    size_t capacity;
} Resource_t;

typedef struct Path_hash_map_t {
    char* path;
    size_t* resource_indices;
    int* is_present;
    size_t capacity;
    size_t size;
} Path_hash_map_t;

extern Path_hash_map_t g_path_to_resource_index_hm;
extern Resource_t* g_resources_array;
extern size_t g_resources_count;
extern size_t g_paths_count;

void init_resources_from_config();

#endif //HTTP_RESOURCES_H

