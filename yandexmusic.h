#ifndef YANDEXMUSIC_H
#define YANDEXMUSIC_H

#include "yandexmusic_global.h"
#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>

typedef struct response{
    char* data;
    size_t len;
}response;

struct artist{
    int id;
    char* name;
};

struct album{
    int id;
    char* name;
};

struct track{
    char* title;
    struct artist* artist;
    struct album* album;
    int id;
    size_t artists_amount;
    size_t albums_amount;
};

typedef struct tracks{
    struct track* item;
    size_t tracks_col;
}tracks;

tracks* yam_search(char*);
size_t writedata(void*, size_t, size_t, response*);
tracks* get_track_info(cJSON*);
void get_download_url(int, char*, int);

#endif /* YANDEXMUSIC_H */
