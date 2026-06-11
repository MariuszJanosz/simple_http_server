#include "http_resources.h"
#include "log.h"
#include "uri.h"
#include "abnf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <unistd.h>

extern char g_www_root[PATH_MAX];

Path_hash_map_t g_path_to_resource_index_hm;
Resource_t* g_resources_array = NULL;
size_t g_resources_count = 0;
size_t g_resources_array_capacity = 1024;

size_t current_line_num = 0;

void init_path_hash_map(size_t capacity) {
    g_path_to_resource_index_hm.capacity = capacity;
    g_path_to_resource_index_hm.count = 0;
    g_path_to_resource_index_hm.buckets =
        calloc(capacity, sizeof(*g_path_to_resource_index_hm.buckets));
    if (!g_path_to_resource_index_hm.buckets) {
        LOG(ERROR, "calloc failed!");
        exit(1);
    }
}

static size_t prehash(const unsigned char* key) {
    size_t res = 0;
    while (*key) {
        res += (unsigned char)*key;
        for (int i = 0; i < 2 * sizeof(res); ++i) {
            res = (res << 4 * i) + res + (res >> 4 * i);
        }
        res += (unsigned char)*key;
        ++key;
    }
    return res;
}

Bucket_t* find_path_in_hash_map(const char* path) {
    size_t hash = prehash(path) % g_path_to_resource_index_hm.capacity;
    size_t original_hash = hash;
    do {
        Bucket_t* b = &g_path_to_resource_index_hm.buckets[hash];
        if (!b->is_present) return NULL;
        if (strcmp(path, b->path) == 0) return b;
        hash += 1;
        hash %= g_path_to_resource_index_hm.capacity;
    } while (hash != original_hash);
    return NULL;
}

ssize_t resource_index_for_path(const char* path) {
    Bucket_t* b = find_path_in_hash_map(path);
    if (b) return b->resource_index;
    //if there is no such path, try locate default resource index
    b = find_path_in_hash_map("*");
    if (b) return b->resource_index;
    //if no default resource defined
    return -1;
}

static void grow_and_rehash() {
    size_t new_cap = 2 * g_path_to_resource_index_hm.capacity;
    Bucket_t* new_buckets = calloc(new_cap, sizeof(*new_buckets));
    if (!new_buckets) {
        LOG(ERROR, "calloc failed!");
        exit(1);
    }
    for (size_t i = 0; i < g_path_to_resource_index_hm.capacity; ++i) {
        if (!g_path_to_resource_index_hm.buckets[i].is_present) continue;
        char* path = g_path_to_resource_index_hm.buckets[i].path;
        size_t ind = g_path_to_resource_index_hm.buckets[i].resource_index;
        size_t hash = prehash(path) % new_cap;
        while (new_buckets[hash].is_present) {
            hash += 1;
            hash %= new_cap;
        }
        new_buckets[hash].path = path;
        new_buckets[hash].resource_index = ind;
        new_buckets[hash].is_present = 1;
    }
    free(g_path_to_resource_index_hm.buckets);
    g_path_to_resource_index_hm.buckets = new_buckets;
    g_path_to_resource_index_hm.capacity = new_cap;
}

void add_path_to_hash_map(const char* path, size_t ind) {
    if (find_path_in_hash_map(path)) {
        LOG(ERROR, "path's resource redefinition at line: %zu.", current_line_num);
        exit(1);
    }
    if ((g_path_to_resource_index_hm.count + 1) / (float)g_path_to_resource_index_hm.capacity > 0.5f)
        grow_and_rehash();
    size_t hash = prehash(path) % g_path_to_resource_index_hm.capacity;
    while (g_path_to_resource_index_hm.buckets[hash].is_present) {
        hash += 1;
        hash %= g_path_to_resource_index_hm.capacity;
    }
    g_path_to_resource_index_hm.buckets[hash].is_present = 1;
    char* s = strdup(path);
    if (!s) {
        LOG(ERROR, "strdup failed!");
        exit(1);
    }
    g_path_to_resource_index_hm.buckets[hash].path = s;
    g_path_to_resource_index_hm.buckets[hash].resource_index = ind;
}

//AD stands for after default.
//Those are states taken after one default resource has been found.
//Finding another defoult resource is an error.
//START is state before parsing.
//RESOURCE is state after parsing resource line
//STATUS is state after parsing status line
//PATH is state after parsing path line
//FIELD_LINE is state after parsing field_line line
//END is state after parsing empty line
//FIN is state after parsing EOF
//If at any point we encounter parsing error we print error diagnostics.
//We can think about it as another ERROR state,
//but there is no reason to add it, since we kill the process anyway.
typedef enum Parser_state_t {
    START,
    RESOURCE,   RESOURCE_AD,
    STATUS,     STATUS_AD,
    PATH,       PATH_AD,
    FIELD_LINE, FIELD_LINE_AD,
    END,        FIN
} Parser_state_t;

static char* get_line(FILE* stream, size_t* len) {
    size_t res_capacity = 128;
    char* res = malloc(res_capacity);
    size_t res_size = 0;
    if (!res) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    while (1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (!feof(stream)) {
                LOG(ERROR, "fgetc failed!");
                exit(1);
            }
            break;
        }
        if (res_size == res_capacity) {
            res_capacity *= 2;
            char* tmp = realloc(res, res_capacity);
            if (!tmp) {
                LOG(ERROR, "realloc failed!");
                exit(1);
            }
            res = tmp;
        }
        res[res_size++] = c;
        if (c == '\n') {
            break;
        }
    }
    if (res_size == res_capacity) {
        res_capacity += 1;
        char* tmp = realloc(res, res_capacity);
        if (!tmp) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        res = tmp;
    }
    res[res_size] = '\0';
    *len = res_size;
    return res;
}

int is_valid_resource(char* str, size_t len) {
    if (len == 0) return 0;
    if (strlen(g_www_root) + len + 1 > PATH_MAX) {
        LOG(ERROR, "path too long at line: %zu.", current_line_num);
        exit(1);
    }
    char resource_path[PATH_MAX];
    strcpy(resource_path, g_www_root);
    strncat(resource_path, str, len);
    if (access(resource_path, F_OK) == 0) {
        return 1;
    }
    else {
        //no such resource or other error
        return 0;
    }
}

void add_resource(char* str, size_t len) {
    if (g_resources_count == g_resources_array_capacity) {
        g_resources_array_capacity *= 2;
        Resource_t* tmp = realloc(g_resources_array,
                g_resources_array_capacity * sizeof(*tmp));
        if (!tmp) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        g_resources_array = tmp;
    }
    char* tar = malloc(PATH_MAX);
    if (!tar) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    strcpy(tar, g_www_root);
    strncat(tar, str, len);
    g_resources_array[g_resources_count].target = tar;
    g_resources_array[g_resources_count].status = -1;
    const size_t init_capacity = 8;
    g_resources_array[g_resources_count].capacity = init_capacity;
    g_resources_array[g_resources_count].count = 0;
    g_resources_array[g_resources_count].field_line_configs =
        malloc(init_capacity * sizeof(Field_line_config_t));
    if (!g_resources_array[g_resources_count].field_line_configs) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    g_resources_count += 1;
}

int is_valid_status(char* str, size_t len) {
    const char* prefix = "\tstatus ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, prefix, prefix_len) == 0) {
        char* status_str = str + prefix_len;
        int status = 0;
        for (int i = 0; i < 3; ++i) {
            if ('0' <= status_str[i] && status_str[i] <= '9') {
                status *= 10;
                status += (status_str[i] - '0');
            }
            else break;
        }
        if (100 <= status && status < 600 &&
                prefix_len + 3 == len) {
            return 1;
        }
    }
    return 0;
}

void add_status(char* str, size_t len) {
    const char* prefix = "\tstatus ";
    size_t prefix_len = strlen(prefix);
    char* status_str = str + prefix_len;
    int status = 0;
    for (int i = 0; i < 3; ++i) {
        status *= 10;
        status += (status_str[i] - '0');
    }
    g_resources_array[g_resources_count - 1].status = status;
}

int is_valid_path(char* str, size_t len, int* is_default) {
    *is_default = 0;
    const char* prefix = "\tpath ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, "\tpath *", len) == 0) {
        *is_default = 1;
        return 1;
    }
    else if (strncasecmp(str, prefix, prefix_len) == 0) {
        char* path = str + prefix_len;
        if (uri_is_path_absolute(path, len - prefix_len)) {
            return 1;
        }
    }
    return 0;
}

void add_path(char* str, size_t len) {
    const char* prefix = "\tpath ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, "\tpath *", len) == 0) {
        add_path_to_hash_map("*", g_resources_count - 1);
    }
    else {
        str[len] = '\0'; //remove trailing '\n' before adding to hm
        add_path_to_hash_map(str + prefix_len, g_resources_count - 1);
    }
}

int is_obs_text(const unsigned char c) {
    return (0x80 <= c && c <= 0xFF);
}

int is_field_value(char* str, size_t len) {
    //empty allowed
    if (len == 0) return 1;
    //if nonempty it has to start and end with VCHAR or obs-text
    //with spaces and HTABs inbetween
    if (!(abnf_is_VCHAR(str[0]) || is_obs_text(str[0]))) return 0;
    for (size_t i = 1; i < len - 1; ++i) {
        if (!(abnf_is_VCHAR(str[i])  || is_obs_text(str[i]) ||
              abnf_is_SP(str[i])    || abnf_is_HTAB(str[i])))
            return 0;
    }
    if (abnf_is_VCHAR(str[len - 1]) || is_obs_text(str[len - 1])) return 1;
    return 0;
}

int is_token(const char* str, size_t len) {
    if (len == 0) return 0;
    for (size_t i = 0; i < len; ++i) {
        if (!(abnf_is_DIGIT(str[i]) || abnf_is_ALPHA(str[i]) ||
              str[i] == '!' || str[i] == '#' || str[i] == '$' ||
              str[i] == '%' || str[i] == '&' || str[i] == '\'' ||
              str[i] == '*' || str[i] == '+' || str[i] == '-' ||
              str[i] == '.' || str[i] == '^' || str[i] == '_' ||
              str[i] == '`' || str[i] == '|' || str[i] == '~'))
            return 0;
    }
    return 1;
}

int is_valid_field_line(char* str, size_t len) {
    const char* prefix = "\tfield_line ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, prefix, prefix_len) == 0) {
        char* field_name = str + prefix_len;
        size_t field_name_len = 0;
        while ( field_name[field_name_len] != ' ' &&
                field_name[field_name_len] != '\0') {
            //normalize to lowercase
            if ('A' <= field_name[field_name_len] &&
                    field_name[field_name_len] <= 'Z') {
                field_name[field_name_len] += 'a' - 'A';
            }
            ++field_name_len;
        }
        if (field_name[field_name_len] == '\0') return 0;
        if (!is_token(field_name, field_name_len)) return 0;
        if (len == prefix_len + field_name_len + 1) return 0;
        const char* function_prefix = "function ";
        size_t function_prefix_len = strlen(function_prefix);
        const char* value_prefix = "value ";
        size_t value_prefix_len = strlen(value_prefix);
        //ptr points to first char after SP after field_name
        char* ptr = field_name + field_name_len + 1;
        if (strncasecmp(ptr, function_prefix,
                    function_prefix_len) == 0) {
            ptr += function_prefix_len;
            if (!(abnf_is_ALPHA(ptr[0]) || ptr[0] == '_')) return 0;
            for (   size_t i = 1;
                    i < len - (prefix_len + field_name_len + 1 + function_prefix_len);
                    ++i) {
                if (!(abnf_is_ALPHA(ptr[i]) || abnf_is_DIGIT(ptr[i]) || ptr[i] == '_'))
                    return 0;
            }
            return 1;
        }
        else if (strncasecmp(ptr, value_prefix, value_prefix_len) == 0) {
            ptr += value_prefix_len;
            if (is_field_value(ptr,
                        len - ( prefix_len +
                                field_name_len + 1 +
                                value_prefix_len))) return 1;
            else return 0;
        }
        else return 0;
    }
    return 0;
}

void add_field_line(char* str, size_t len) {
    Resource_t* resource = &g_resources_array[g_resources_count - 1];
    if (resource->count == resource->capacity) {
        resource->capacity *= 2;
        Field_line_config_t* tmp = realloc(resource->field_line_configs,
                resource->capacity * sizeof(*tmp));
        if (!tmp) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        resource->field_line_configs = tmp;
    }
    Field_line_config_t* flc = &resource->field_line_configs[resource->count++];
    flc->field_name = NULL;
    flc->func = NULL;
    flc->field_value = NULL;
    const char* prefix = "\tfield_line ";
    size_t prefix_len = strlen(prefix);
    char* field_name = str + prefix_len;
    size_t field_name_len = 0;
    while ( field_name[field_name_len] != ' ' &&
            field_name[field_name_len] != '\0')
        ++field_name_len;
    char* name = strndup(field_name, field_name_len);
    if (!name) {
        LOG(ERROR, "strndup failed!");
        exit(1);
    }
    flc->field_name = name;
    const char* function_prefix = "function ";
    size_t function_prefix_len = strlen(function_prefix);
    const char* value_prefix = "value ";
    size_t value_prefix_len = strlen(value_prefix);
    //ptr points to first char after SP after field_name
    char* ptr = field_name + field_name_len + 1;
    if (strncasecmp(ptr, function_prefix, function_prefix_len) == 0) {
        ptr += function_prefix_len;
        char* func = strndup(ptr, len - (prefix_len + field_name_len + 1 + function_prefix_len));
        if (!func) {
            LOG(ERROR, "strndup failed!");
            exit(1);
        }
        flc->func = func;
    }
    else {
        ptr += value_prefix_len;
        char* field_value = strndup(ptr, len - (prefix_len + field_name_len + 1 + value_prefix_len));
        if (!field_value) {
            LOG(ERROR, "strndup failed!");
            exit(1);
        }
        flc->field_value = field_value;
    }
}

void init_resources_from_config() {
    init_path_hash_map(1024);
    g_resources_array = malloc(g_resources_array_capacity *
            sizeof(*g_resources_array));
    if (!g_resources_array) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    if (strlen(g_www_root) + strlen("/.config") + 1 > PATH_MAX) {
        LOG(ERROR, "path to .config file too long!");
        exit(1);
    }
    char path_to_config[PATH_MAX];
    strcpy(path_to_config, g_www_root);
    strcat(path_to_config, "/.config");
    FILE* config_file = fopen(path_to_config, "r");
    if (!config_file) {
        LOG(ERROR, "fopen failed!");
        exit(1);
    }
    Parser_state_t state = START;
    while (state != FIN) {
        size_t line_len;
        char* line = get_line(config_file, &line_len);
        current_line_num += 1;
        int is_EOF = feof(config_file);
        switch (state) {
            case START:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (resource LF) "
                                    "| LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (strcmp(line, "\n") == 0) {
                        state = END;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        state = RESOURCE;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (resource LF) "
                                    "| LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case RESOURCE:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"status\" "
                                    "SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_status(line, line_len - 1)) {
                        add_status(line, line_len - 1);
                        state = STATUS;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"status\" "
                                    "SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case RESOURCE_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"status\" "
                                    "SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_status(line, line_len - 1)) {
                        add_status(line, line_len - 1);
                        state = STATUS_AD;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"status\" "
                                    "SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case STATUS:
                {
                    int is_default = 0;
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"path\" "
                                    "SP (request_path | \"*\") "
                                    "LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1, &is_default)) {
                        add_path(line, line_len - 1);
                        if (is_default) state = PATH_AD;
                        else state = PATH;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"path\" "
                                    "SP (request_path | \"*\") "
                                    "LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case STATUS_AD:
                {
                    int is_default = 0;
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"path\" "
                                    "SP (request_path | \"*\") "
                                    "LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1, &is_default)) {
                        if (is_default) {
                            LOG(ERROR,  "Default resource "
                                        "redefinition at line: "
                                        "%zu.\nGot: \"%s\".",
                                        current_line_num,
                                        line);
                            exit(1);
                        }
                        else {
                            add_path(line, line_len - 1);
                            state = PATH_AD;
                        }
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: TAB \"path\" "
                                    "SP (request_path | \"*\") "
                                    "LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case PATH:
                {
                    int is_default = 0;
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (TAB \"path\" SP "
                                    "(request_path | \"*\") LF) "
                                    "| (TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1, &is_default)) {
                        add_path(line, line_len - 1);
                        if (is_default) state = PATH_AD;
                        else state = PATH;
                    }
                    else if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        state = FIELD_LINE;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        state = RESOURCE;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (TAB \"path\" SP "
                                    "(request_path | \"*\") LF) "
                                    "| (TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case PATH_AD:
                {
                    int is_default = 0;
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (TAB \"path\" SP "
                                    "(request_path | \"*\") LF) "
                                    "| (TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1, &is_default)) {
                        if (is_default) {
                            LOG(ERROR,  "Default resource "
                                        "redefinition at line: "
                                        "%zu.\nGot: \"%s\".",
                                        current_line_num,
                                        line);
                            exit(1);
                        }
                        else {
                            add_path(line, line_len - 1);
                            state = PATH_AD;
                        }
                    }
                    else if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        state = FIELD_LINE_AD;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        state = RESOURCE_AD;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: (TAB \"path\" SP "
                                    "(request_path | \"*\") LF) "
                                    "| (TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case FIELD_LINE:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: "
                                    "(TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        state = FIELD_LINE;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        state = RESOURCE;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: "
                                    "(TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case FIELD_LINE_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: "
                                    "(TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        state = FIELD_LINE_AD;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        state = RESOURCE_AD;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: "
                                    "(TAB \"field_line\" SP "
                                    "field_name SP "
                                    "(\"function\" SP func "
                                    "| \"value\" SP field_value) "
                                    "LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case END:
                {
                    if (is_EOF && line[0] == '\0') {
                        state = FIN;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: "
                                    "%zu.\nGot: \"%s\".\n"
                                    "Expected: EOF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
        }
        free(line);
    }
}

