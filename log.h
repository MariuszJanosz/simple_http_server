#ifndef LOG_H
#define LOG_H

#include <stdio.h>

typedef enum Log_types_t {
    INFO,
    ERROR
} Log_types_t;

#define stringify(x) _stringify_(x)
#define _stringify_(x) #x
#ifndef NDEBUG
#define LOG(type, msg) fprintf(stderr, "[%s]: FILE: %s LINE: %s; %s\n", #type, __FILE__, stringify(__LINE__), msg)
#else
#define LOG(type, msg) 
#endif //NDEBUG

#ifndef NDEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif //NDEBUG

#endif //LOG_H

