#include "http_resources.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

extern char g_www_root[PATH_MAX];

Path_hash_map_t g_path_to_resource_index_hm;
Resource_t* g_resources_array;
size_t g_resources_count;
size_t g_paths_count;

//AD stands for after default.
//Those are states taken after one default resource has been found.
//Finding another defoult resource is an error.
typedef enum Parser_state_t {
    START,
    RESOURCE,   RESOURCE_AD,
    STATUS,     STATUS_AD,
    PATH,       PATH_AD,
    FIELD_LINE, FIELD_LINE_AD,
    END,        FIN
} Parser_state_t;

static char* get_line(FILE* stream) {
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
    return res;
}

void init_resources_from_config() {
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
        char* line = get_line(config_file);
        switch (state) {
            case START:
                {
                
                
                }
                break;
            case RESOURCE:
                {
                
                
                }
                break;
            case RESOURCE_AD:
                {
                
                
                }
                break;
            case STATUS:
                {
                
                
                }
                break;
            case STATUS_AD:
                {
                
                
                }
                break;
            case PATH:
                {
                
                
                }
                break;
            case PATH_AD:
                {
                
                
                }
                break;
            case FIELD_LINE:
                {
                
                
                }
                break;
            case FIELD_LINE_AD:
                {
                
                
                }
                break;
            case END:
                {
                
                
                }
                break;
            case FIN:
                {
                
                
                }
                break;
        }
    }
}

