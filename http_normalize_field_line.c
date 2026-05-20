#include "http_normalize_field_line.h"

#include <string.h>

Field_line_type_t determine_type(const char* field_name) {
    
}

Http_status_t normalize_list(Field_line_t* fl) {

}

Http_status_t normalize_singleton(Field_line_t* fl) {

}

Http_status_t normalize_singleton_with_deduplication(Field_line_t* fl) {

}

Http_status_t normalize_field_line(Field_line_t* fl) {
    const char* field_name = fl->field_name;
    Field_line_type_t type = determine_type(field_name);
    switch (type) {
        case LIST:
            return normalize_list(fl);
        case SINGLETON:
            return normalize_singleton(fl);
        case SINGLETON_WITH_DEDUPLICATION:
            return normalize_singleton_with_deduplication(fl);
        case UNKNOWN_TYPE:
            break; //Do not normalize unknown types
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

