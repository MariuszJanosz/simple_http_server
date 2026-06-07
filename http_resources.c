#include "http_resources.h"
#include "log.h"
#include "uri.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <unistd.h>

extern char g_www_root[PATH_MAX];

Path_hash_map_t g_path_to_resource_index_hm;
Resource_t* g_resources_array = NULL;
size_t g_resources_count = 0;

void init_path_hash_map(size_t capacity) {
    g_path_to_resource_index_hm.capacity = capacity;
    g_path_to_resource_index_hm.size = 0;
    g_path_to_resource_index_hm.buckets =
        calloc(capacity, sizeof(*g_path_to_resource_index_hm.buckets));
    if (!g_path_to_resource_index_hm.buckets) {
        LOG(ERROR, "calloc failed!");
        exit(1);
    }
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
        LOG(ERROR, "path too long!");
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

}

int is_valid_path(char* str, size_t len) {
    const char* prefix = "\tpath ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, "\tpath *", len) == 0) return 1;
    else if (strncasecmp(str, prefix, prefix_len) == 0) {
        char* path = str + prefix_len;
        if (uri_is_path_absolute(path, len - prefix_len)) {
            return 1;
        }
    }
    return 0;
}

void add_path(char* str, size_t len, int* is_default) {

}

int is_valid_field_line(char* str, size_t len) {
    const char* prefix = "\tfield_line ";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, prefix, prefix_len) == 0) {
        //TODO
        return 1;
    }
    return 0;
}

void add_field_line(char* str, size_t len) {

}

void init_resources_from_config() {
    init_path_hash_map(1024);
    size_t resources_array_capacity = 1024;
    g_resources_array = malloc(resources_array_capacity *
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
    size_t current_line_num = 0;
    while (state != FIN) {
        size_t line_len;
        char* line = get_line(config_file, &line_len);
        current_line_num += 1;
        int is_EOF = feof(config_file);
        switch (state) {
            case START:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (strcmp(line, "\n") == 0) {
                        free(line);
                        state = END;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        free(line);
                        state = RESOURCE;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case RESOURCE:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: TAB \"status\" SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_status(line, line_len - 1)) {
                        add_status(line, line_len - 1);
                        free(line);
                        state = STATUS;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: TAB \"status\" SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case RESOURCE_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: TAB \"status\" SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_status(line, line_len - 1)) {
                        add_status(line, line_len - 1);
                        free(line);
                        state = STATUS_AD;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: TAB \"status\" SP STATUS LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case STATUS:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: TAB \"path\" SP (request_path | \"*\") LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1)) {
                        int is_default = 0;
                        add_path(line, line_len - 1, &is_default);
                        free(line);
                        if (is_default) state = PATH_AD;
                        else state = PATH;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: TAB \"path\" SP (request_path | \"*\") LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case STATUS_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: TAB \"path\" SP (request_path | \"*\") LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1)) {
                        int is_default = 0;
                        add_path(line, line_len - 1, &is_default);
                        free(line);
                        if (is_default) {
                            LOG(ERROR,  "Default resource redefinition at line: %zu.\nGot: \"%s\".",
                                        current_line_num,
                                        line);
                            exit(1);
                        }
                        else state = PATH_AD;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: TAB \"path\" SP (request_path | \"*\") LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case PATH:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"path\" SP (request_path | \"*\") LF) | (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1)) {
                        int is_default = 0;
                        add_path(line, line_len - 1, &is_default);
                        free(line);
                        if (is_default) state = PATH_AD;
                        else state = PATH;
                    }
                    else if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        free(line);
                        state = FIELD_LINE;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        free(line);
                        state = RESOURCE;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        free(line);
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"path\" SP (request_path | \"*\") LF) | (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case PATH_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"path\" SP (request_path | \"*\") LF) | (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_path(line, line_len - 1)) {
                        int is_default = 0;
                        add_path(line, line_len - 1, &is_default);
                        free(line);
                        if (is_default) {
                            LOG(ERROR,  "Default resource redefinition at line: %zu.\nGot: \"%s\".",
                                        current_line_num,
                                        line);
                            exit(1);
                        }
                        else state = PATH_AD;
                    }
                    else if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        free(line);
                        state = FIELD_LINE_AD;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        free(line);
                        state = RESOURCE_AD;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        free(line);
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"path\" SP (request_path | \"*\") LF) | (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case FIELD_LINE:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        free(line);
                        state = FIELD_LINE;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        free(line);
                        state = RESOURCE;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        free(line);
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case FIELD_LINE_AD:
                {
                    if (is_EOF) {
                        LOG(ERROR,  "Unexpected EOF at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                    if (is_valid_field_line(line, line_len - 1)) {
                        add_field_line(line, line_len - 1);
                        free(line);
                        state = FIELD_LINE_AD;
                    }
                    else if (is_valid_resource(line, line_len - 1)) {
                        add_resource(line, line_len - 1);
                        free(line);
                        state = RESOURCE_AD;
                    }
                    else if (strcmp(line, "\n") == 0) {
                        free(line);
                        state = END;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: (TAB \"field_line\" SP field_name SP (\"function\" SP func | \"value\" SP field_value) LF) | (resource LF) | LF",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
            case END:
                {
                    if (is_EOF && line[0] == '\0') {
                        free(line);
                        state = FIN;
                    }
                    else {
                        LOG(ERROR,  "Unexpected input at line: %zu.\nGot: \"%s\".\nExpected: EOF.",
                                    current_line_num,
                                    line);
                        exit(1);
                    }
                }
                break;
        }
    }

}

