#include "http_field_line.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

size_t prehash(const unsigned char* key) {
    size_t res = 0;
    while (*key) {
        res = (res << (4 * sizeof(size_t)) + *key) ^ (res >> (4 * sizeof(size_t)));
        ++key;
    }
    return res;
}

void init_field_line_hash_map(Field_line_hash_map_t* hm, const size_t capacity) {
    hm->buckets = calloc(capacity, sizeof(*hm->buckets));
    if (!hm->buckets) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    hm->capacity = capacity;
    hm->occupied_count = 0;
}

void free_field_line_hash_map(Field_line_hash_map_t* hm) {
    for (size_t i = 0; i < hm->capacity; ++i) {
        if (hm->buckets[i].bucket_status == OCCUPIED) {
            free(hm->buckets[i].field_line.field_name);
            free(hm->buckets[i].field_line.field_value);
        }
    }
    free(hm->buckets);
}

ssize_t find_field_line_bucket_index(const Field_line_hash_map_t* hm, const char* field_name) {
    size_t start_index = prehash(field_name) % hm->capacity;
    size_t res = start_index;
    do {
        if (hm->buckets[res].bucket_status == OCCUPIED &&
                strcmp(hm->buckets[res].field_line.field_name, field_name) == 0) {
            return res;
        }
        ++res;
        res %= hm->capacity;
    } while (hm->buckets[res].bucket_status != EMPTY && res != start_index);
    return -1;
}

Field_line_t* find_field_line_in_hash_map(const Field_line_hash_map_t* hm, const char* field_name) {
    ssize_t index = find_field_line_bucket_index(hm, field_name);
    if (index < 0) return NULL;
    return &hm->buckets[(size_t)index].field_line;
}

void grow_and_rehash(Field_line_hash_map_t* hm) {
    //Allocate new array with twice the capacity
    size_t new_capacity = 2 * hm->capacity;
    Field_line_hash_map_bucket_t* new_buckets = calloc(new_capacity, sizeof(*new_buckets));
    if (!new_buckets) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    //Move elements to new_buckets array
    for (size_t i = 0; i < hm->capacity; ++i) {
        if (hm->buckets[i].bucket_status != OCCUPIED) continue;
        size_t hash = prehash(hm->buckets[i].field_line.field_value) % new_capacity;
        while (new_buckets[hash].bucket_status != EMPTY) {
            ++hash;
            hash %= new_capacity;
        }
        new_buckets[hash].bucket_status = OCCUPIED;
        new_buckets[hash].field_line = hm->buckets[i].field_line;
    }
    //Free old array and make hm to point to new array
    free(hm->buckets);
    hm->buckets = new_buckets;
    hm->capacity = new_capacity;
}

void add_field_line_to_hash_map(Field_line_hash_map_t* hm, const Field_line_t field_line) {
    //If this key is already present append value after ','
    ssize_t index = find_field_line_bucket_index(hm, field_line.field_name);
    if (index >= 0) {
        char* old_fv = hm->buckets[index].field_line.field_value;
        //old_fv + ',' + field_line.field_value + '\0'
        //new_fv_len + 1 ('\0') = old_fv_len + 1 + appended_fv_len + 1
        size_t old_fv_len = strlen(old_fv);
        size_t appended_fv_len = strlen(field_line.field_value);
        char* new_fv = realloc(old_fv, old_fv_len + 1 + appended_fv_len + 1);
        if (!new_fv) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        new_fv[old_fv_len] = ',';
        new_fv[old_fv_len + 1] = '\0';
        strcat(new_fv, field_line.field_value);
        hm->buckets[index].field_line.field_value = new_fv;
        return;
    }
#define LOAD_FACTOR_REHASH_THRESHOLD 0.5f
    if ((float)(hm->occupied_count + 1) / hm->capacity > LOAD_FACTOR_REHASH_THRESHOLD)
        grow_and_rehash(hm);
    size_t hash = prehash(field_line.field_name) % hm->capacity;
    while (hm->buckets[hash].bucket_status == OCCUPIED) {
        ++hash;
        hash %= hm->capacity;
    }
    hm->buckets[hash].bucket_status = OCCUPIED;
    char* s = strdup(field_line.field_name);
    if (!s) {
        LOG(ERROR, "strdup failed!");
        exit(1);
    }
    hm->buckets[hash].field_line.field_name = s;
    s = strdup(field_line.field_value);
    if (!s) {
        LOG(ERROR, "strdup failed!");
        exit(1);
    }
    hm->buckets[hash].field_line.field_value = s;
    hm->occupied_count += 1;
}

