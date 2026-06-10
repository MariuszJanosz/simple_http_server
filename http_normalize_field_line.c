#include "http_normalize_field_line.h"
#include "http_common_field_lines.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

Field_line_type_t determine_type(const char* field_name) {
    return find_field_line_attributes(field_name)->type;
}

void combine_values_into_list(Field_line_t* fl) {
    //nothing to do, so return early to avoid malloc
    if (fl->count < 2) return;
    size_t new_line_size = 0;
    size_t lengths[fl->count];
    for (int i = 0; i < fl->count; ++i) {
        //+1 for appended ',' or '\0' after each appended line
        lengths[i] = strlen(fl->field_values[i]);
        new_line_size += lengths[i] + 1;
    }
    char* new_line = malloc(new_line_size);
    if (!new_line) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t first_empty = 0;
    for (int i = 0; i < fl->count; ++i) {
        strcpy(new_line + first_empty, fl->field_values[i]);
        first_empty += lengths[i];
        free(fl->field_values[i]);
        new_line[first_empty++] = ',';
    }
    new_line[first_empty - 1] = '\0';
    fl->count = 1;
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
        return PARSING_FINE;
    }
    do {
        if (strcmp(first_token, next_token) != 0) {
            if (strcasecmp("Content-Length", fl->field_name) == 0)
                return PARSING_BROKEN_CLOSE_CONNECTION;
            return HTTP_STATUS_BAD_REQUEST;
        }
    } while (next_token = strtok(NULL, ","));
    return PARSING_FINE;
}

Http_status_t normalize_multiline_do_not_merge(Field_line_t* fl) {
    return PARSING_FINE; //do nothing
}

Http_status_t normalize_unknown_type(Field_line_t* fl) {
    return PARSING_FINE; //do nothing
}

void remove_OWS_list(Field_line_t* fl) {
    for (size_t i = 0; i < fl->count; ++i) {
        char* new_start = fl->field_values[i];
        while (*new_start == ' ' || *new_start == '\t') {
            ++new_start;
        }
        size_t len = strlen(new_start);
        while (len > 0 && (new_start[len - 1] == ' ' || new_start[len - 1] == '\t')) --len;
        new_start[len] = '\0';
        len = 0;
        size_t OWS_count = 0;
        while (new_start[len]) {
            char* first_empty = new_start + len;
            char* next_section = first_empty + OWS_count;
            while (*next_section == ' ' || *next_section == '\t') {
                ++next_section;
                ++OWS_count;
            }
            if (*next_section == '\0') { //It was the last section
                *first_empty = '\0';
                break;
            }
            else if (*next_section == '\"') {
                size_t section_len = 1;
                while ( next_section[section_len] != '\"' &&
                        next_section[section_len] != '\0') {
                    ++section_len;
                }
                //If it was invalid return immediately, further parsing would return error anyway
                if (next_section[section_len] == '\0') return;
                ++section_len;
                size_t section_OWS_count = 0;
                while ( next_section[section_len + section_OWS_count] == ' ' ||
                        next_section[section_len + section_OWS_count] == '\t') {
                    ++section_OWS_count;
                }
                //If it was invalid return immediately, further parsing would return error anyway
                if (    next_section[section_len + section_OWS_count] != '\0' &&
                        next_section[section_len + section_OWS_count] != ',') {
                    return;
                }
                if (next_section[section_len + section_OWS_count] == ',') {
                    memmove(first_empty, next_section, section_len);
                    first_empty[section_len] = ',';
                    len += section_len + 1;
                    OWS_count += section_OWS_count;
                }
                else {
                    memmove(first_empty, next_section, section_len);
                    first_empty[section_len] = '\0';
                    len += section_len;
                    OWS_count += section_OWS_count;
                }
            }
            else if (*next_section == ',') {
                *first_empty = ',';
                ++len;
            }
            else {
                size_t section_len = 1;
                while ( next_section[section_len] != '\0' &&
                        next_section[section_len] != ',') {
                    ++section_len;
                }
                size_t section_OWS_count = 0;
                if (next_section[section_len] == '\0') {
                    while (section_len > 0 && ( next_section[section_len - 1] == ' ' ||
                                                next_section[section_len - 1] == '\t')) {
                        --section_len;
                        ++section_OWS_count;
                    }
                    memmove(first_empty, next_section, section_len);
                    first_empty[section_len] = '\0';
                    len += section_len;
                    OWS_count += section_OWS_count;
                }
                else {
                    while (section_len > 0 && ( next_section[section_len - 1] == ' ' ||
                                                next_section[section_len - 1] == '\t')) {
                        --section_len;
                        ++section_OWS_count;
                    }
                    memmove(first_empty, next_section, section_len);
                    first_empty[section_len] = ',';
                    len += section_len + 1;
                    OWS_count += section_OWS_count;
                }
            }
        }
        memmove(fl->field_values[i], new_start, len + 1);
    }
}

void remove_OWS_singleton(Field_line_t* fl) {
    for (size_t i = 0; i < fl->count; ++i) {
        char* ptr = fl->field_values[i];
        char* first_empty = ptr;
        while (*ptr == ' ' || *ptr == '\t') {
            ++ptr;
        }
        if (*ptr == '\0') {
            *first_empty = '\0';
            return;
        }
        else {
            size_t trailing_spaces = 0;
            while (*ptr != '\0') {
                *first_empty = *ptr;
                if (*ptr == ' ' || *ptr == '\t') ++trailing_spaces;
                else trailing_spaces = 0;
                ++first_empty;
                ++ptr;
            }
            first_empty[-trailing_spaces] = '\0';
        }
    }
}

void remove_OWS(Field_line_t* fl, Field_line_type_t type) {
    switch (type) {
        case LIST:
        case LIST_NONEMPTY:
        case SINGLETON_WITH_DEDUPLICATION:
            remove_OWS_list(fl);
            break;
        case SINGLETON:
            remove_OWS_singleton(fl);
            break;
        case MULTILINE_DO_NOT_MERGE:
        case UNKNOWN_TYPE:
        default:
            break; //do nothing
    }
}

Http_status_t normalize_field_line(Field_line_t* fl) {
    const char* field_name = fl->field_name;
    Field_line_type_t type = determine_type(field_name);
    //first we have to normalize OWSs otherwise byte by byte deduplication causes trouble
    //we normalize them by removing them, it simplifies further processing like for example
    //we don't have to skip white spaces when converting Content-Length from string to number
    remove_OWS(fl, type);
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
        default:
            return normalize_unknown_type(fl);
    }
    return PARSING_FINE;
}

Http_status_t normalize(Field_line_hash_map_t* hm) {
    Http_status_t res = PARSING_FINE;
    for (size_t i = 0; i < hm->capacity; ++i) {
        if (hm->buckets[i].bucket_status != OCCUPIED) continue;
        Http_status_t tmp = normalize_field_line(&hm->buckets[i].field_line);
        if (tmp == PARSING_BROKEN_CLOSE_CONNECTION) {
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        if (res == PARSING_FINE) res = tmp;
    }
    return res;
}

