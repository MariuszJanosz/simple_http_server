#include "http_normalize_field_line.h"
#include "http_common_field_lines.h"

#include <string.h>

Field_line_type_t determine_type(const char* field_name) {
    return find_field_line_attributes(field_name)->type;
}

void combine_values_into_list(Field_line_t* fl) {
    //nothing to do, so return early to avoid malloc
    if (fl->size < 2) return;
    size_t new_line_size = 0;
    for (int i = 0; i < fl->size; ++i) {
        //+1 for appended ',' or '\0' after each appended line
        new_line_size += strlen(fl->field_values[i]) + 1;
    }
    char* new_line = malloc(new_line_size);
    if (!new_line) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t first_empty = 0;
    for (int i = 0; i < fl->size; ++i) {
        strcpy(new_line + first_empty, fl->field_values[i]);
        first_empty += strlen(fl->field_values[i]);
        free(fl->field_values[i]);
        new_line[first_empty++] = ',';
    }
    new_line[first_empty - 1] = '\0';
    fl->size = 1;
    fl->field_values[0] = new_line;
}

Http_status_t normalize_list(Field_line_t* fl) {
    combine_values_into_list(fl);
    return PARSING_FINE;
}

Http_status_t normalize_list_nonempty(Field_line_t* fl) {
    return normalize_list(fl);
}

Http_status_t normalize_singleton(Field_line_t* fl) {
    if (fl->count > 1) {
        return HTTP_STATUS_BAD_REQUEST;
    }
    return PARSING_FINE;
}

Http_status_t normalize_singleton_with_deduplication(Field_line_t* fl) {
    combine_values_into_list(fl);
    char* first_token = strtok(fl->field_values[0], ",");
    char* next_token = strtok(NULL, ",");
    if (!next_token) {
        //If there was only one toke we return early to eliminate strdup
        return PARSING_FINE;
    }
    do {
        if (strcmp(first_token, next_token) != 0) {
            if (strcasecmp("Content-Length", fl->field_name) == 0)
                return PARSING_BROKEN_CLOSE_CONNECTION;
            return HTTP_STATUS_BAD_REQUEST;
        }
    } while (next_token = strtok(NULL, ","));
    char* deduplicated = strdup(first_token);
    if (!deduplicated) {
        LOG(ERROR, "strdup failed!");
        exit(1);
    }
    free(fl->field_values[0]);
    fl->field_values[0] = deduplicated;
    return PARSING_FINE;
}

Http_status_t normalize_multiline_do_not_merge(Field_line_t* fl) {
    return PARSING_FINE; //do nothing
}

Http_status_t normalize_unknown_type(Field_line_t* fl) {
    return PARSING_FINE; //do nothing
}

Http_status_t normalize_field_line(Field_line_t* fl) {
    const char* field_name = fl->field_name;
    Field_line_type_t type = determine_type(field_name);
    switch (type) {
        case LIST:
            return normalize_list(fl);
        case LIST_NONEMPTY:
            return normalize_list_nonempty(fl);
        case SINGLETON:
            return normalize_singleton(fl);
        case SINGLETON_WITH_DEDUPLICATION:
            return normalize_singleton_with_deduplication(fl);
        case MULTILINE_DO_NOT_MERGE:
            return normalize_multiline_do_not_merge(fl);
        case UNKNOWN_TYPE:
            return normalize_unknon_type(fl);
    }
    return PARSING_FINE;
}

Http_status_t normalize(Http_field_line_hash_map_t* hm) {
    Http_status_t res = PARSING_FINE;
    for (size_t i = 0; i < hm->count; ++i) {
        if (hm->buckets[i] != OCCUPIED) continue;
        Http_status_t tmp = normalize_field_line(&hm->buckets[i].field_line);
        if (tmp == PARING_BROKEN_CLOSE_CONNECTION) {
            return PARING_BROKEN_CLOSE_CONNECTION;
        }
        if (res == PARING_FINE) res = tmp;
    }
    return res;
}

