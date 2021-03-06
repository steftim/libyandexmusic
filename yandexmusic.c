/*
    libyandexmusic
    Copyright (C) 2020  Steftim
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA
*/

#include "yandexmusic.h"
#include "inside.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <json-c/json.h>
static CURL* curl;
CURLcode res;

static track* unjson_track_info(response response);

tracks* yam_search(char* query, userInfo* userinfo, char* proxy, char* proxy_type){
    response response;
    struct tracks* tracks_info = NULL;
    response.len = 0;
    response.data = NULL;
    curl = curl_easy_init();
    if(curl){
        char* ptr = strchr(query, ' ');
        while(ptr){ *ptr = '+'; ptr = strchr(query, ' '); }

        size_t query_len = strlen(query) + 99;
        char* search_query = calloc(query_len, sizeof(char));
        snprintf(search_query, query_len, "%s%s%s", "https://api.music.yandex.net/search?text=", query, "&nocorrect=false&type=all&page=0&playlist-in-best=true");

        curl_easy_setopt(curl, CURLOPT_URL, search_query);
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows 10");
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
        //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "X-Yandex-Music-Client: WindowsPhone/4.20");

        size_t tokenstrlen = 0;
        char* tokenstr = NULL;
        if(userinfo->access_token != NULL){
            tokenstrlen = strlen("Authorization: OAuth ") + strlen(userinfo->access_token) + 1;

            tokenstr = calloc(tokenstrlen, sizeof(char));
            snprintf(tokenstr, tokenstrlen, "Authorization: OAuth %s", userinfo->access_token);
            headers = curl_slist_append(headers, tokenstr);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s, at yam_search()\n", curl_easy_strerror(res));
        }

        free(search_query);
        if(!response.data){goto end;}

        int status;
        struct json_object* json_resp,* tracks,* result,* results;
        json_resp = json_tokener_parse(response.data);
        json_object_object_get_ex(json_resp, "result", &result);
        json_object_object_get_ex(result, "tracks", &tracks);
        status = json_object_object_get_ex(tracks, "results", &results);
        if(status == 0){
            printf("\n\nError:\t%s\n\n", response.data);
#ifdef DEBUG
            if(tokenstr != NULL)printf("token str: %s\ttoken str len:  %d\n\n", tokenstr, (int)tokenstrlen);
            printf("token:  %s\n\n", userinfo->access_token);
#endif
            goto end;
        }
        curl_easy_cleanup(curl);
        tracks_info = get_tracks_info(results);
    }

end:
    free(response.data);
    return tracks_info;
}

/* curl */
size_t writedata(void* data, size_t size, size_t nmemb, struct response* userdata){
    size_t new_len = userdata->len + (size*nmemb);
    if(userdata->len != 0){
        userdata->data = realloc(userdata->data, (new_len + 1) * sizeof(char));
    }else{
        userdata->data = calloc(new_len + 1, sizeof(char));
    }
    memcpy(userdata->data + userdata->len, data, size*nmemb);
    userdata->data[new_len] = '\0';
    userdata->len = new_len;

    return size*nmemb;
}

download* get_link(response response){
    json_object* json,* result;
    json = json_tokener_parse(response.data);
    result = json_object_object_get(json, "result");
    size_t arr_size = json_object_array_length(result);
    download* tmp = calloc(arr_size, sizeof(download));
    uint i;
    for(i = 0; i < arr_size; i++){
        json_object* item,* codec,* gain,* preview,* downloadInfoUrl,* direct,* bitrateInKbps;
        item = json_object_array_get_idx(result, i);
        codec = json_object_object_get(item, "codec");
        gain = json_object_object_get(item, "gain");
        preview = json_object_object_get(item, "preview");
        downloadInfoUrl = json_object_object_get(item, "downloadInfoUrl");
        direct = json_object_object_get(item, "direct");
        bitrateInKbps = json_object_object_get(item, "bitrateInKbps");

        tmp[i].codec = calloc(json_object_get_string_len(codec), sizeof(char));
        tmp[i].downloadInfoUrl = calloc(json_object_get_string_len(bitrateInKbps), sizeof(char));

        tmp[i].codec = (char*)json_object_get_string(codec);
        tmp[i].gain = json_object_get_boolean(gain);
        tmp[i].preview = json_object_get_boolean(preview);
        tmp[i].downloadInfoUrl = (char*)json_object_get_string(downloadInfoUrl);
        tmp[i].direct = json_object_get_boolean(direct);
        tmp[i].bitrateInKbps = json_object_get_int(bitrateInKbps);
    }
    free(json);
    return tmp;
}

tracks* get_tracks_info(json_object* input_info){
    size_t trackItm_s = json_object_array_length(input_info);
    struct tracks* tmp = malloc(sizeof(tracks));
    tmp->item = calloc(trackItm_s, sizeof(struct track));
    tmp->tracks_col = trackItm_s;

    uint i, j;
    for(i = 0; i < tmp->tracks_col; i++){
        json_object* track,* title,* id,* albums,* artists;
        track = json_object_array_get_idx(input_info, i);
        title = json_object_object_get(track, "title");
        id = json_object_object_get(track, "id");

        albums = json_object_object_get(track, "albums");
        artists = json_object_object_get(track, "artists");

        tmp->item[i].albums_amount = json_object_array_length(albums);
        tmp->item[i].artists_amount = json_object_array_length(artists);
        tmp->item[i].album = calloc(tmp->item[i].albums_amount, sizeof(struct album));
        tmp->item[i].artist = calloc(tmp->item[i].artists_amount, sizeof(struct artist));

        if(title){
            size_t title_s = json_object_get_string_len(title);
            tmp->item[i].title = calloc(title_s + 1, sizeof(char));
            tmp->item[i].title = (char*)json_object_get_string(title);
        }
        tmp->item[i].id = json_object_get_int(id);

        for(j = 0; j < tmp->item[i].artists_amount; j++){
            json_object* artist_item,* ari_name,* ari_id;
            artist_item = json_object_array_get_idx(artists, j);
            ari_name = json_object_object_get(artist_item, "name");
            ari_id = json_object_object_get(artist_item, "id");
            if(ari_name){
                size_t artist_s = json_object_get_string_len(ari_name);
                tmp->item[i].artist[j].name = calloc(artist_s + 1, sizeof(char));
                tmp->item[i].artist[j].name = (char*)json_object_get_string(ari_name);
            }
            if(ari_id)tmp->item[i].artist[j].id = json_object_get_int(ari_id);
        }
    }
    return tmp;
}

track* get_track_info_from_id(uint id, userInfo* userinfo, char* proxy, char* proxy_type){
    response response;
    struct track* track_info = NULL;
    response.len = 0;
    response.data = NULL;
    curl = curl_easy_init();
    if(curl){
        size_t query_len = 50;
        char* search_query = calloc(query_len, sizeof(char));
        snprintf(search_query, query_len, "%s%d", "https://api.music.yandex.net/tracks/", id);

        curl_easy_setopt(curl, CURLOPT_URL, search_query);
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows 10");
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "X-Yandex-Music-Client: WindowsPhone/4.20");

        size_t tokenstrlen = 0;
        char* tokenstr = NULL;
        if(userinfo->access_token != NULL){
            tokenstrlen = strlen("Authorization: OAuth ") + strlen(userinfo->access_token) + 1;
            tokenstr = calloc(tokenstrlen, sizeof(char));
            snprintf(tokenstr, tokenstrlen, "Authorization: OAuth %s", userinfo->access_token);
            headers = curl_slist_append(headers, tokenstr);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s, at get_track_info_from_id()\n", curl_easy_strerror(res));
        }
        free(search_query);
        if(!response.data){goto end;}
        curl_easy_cleanup(curl);
        track_info = unjson_track_info(response);
    }
end:
    free(response.data);

    return track_info;
}

static track* unjson_track_info(response response){
    int status;
    struct json_object* json_resp,* result;
    json_resp = json_tokener_parse(response.data);
    status = json_object_object_get_ex(json_resp, "result", &result);
    if(status == 0){
        printf("\n\nError:\t%s\n\n", response.data);
        goto end;
    }
    track* tmp = calloc(1, sizeof(track));
    json_object* track,* title,* id,* albums,* artists;
    uint k, j;

    track = json_object_array_get_idx(result, 0);
    title = json_object_object_get(track, "title");
    id = json_object_object_get(track, "id");

    albums = json_object_object_get(track, "albums");
    artists = json_object_object_get(track, "artists");

    tmp->albums_amount = json_object_array_length(albums);
    tmp->artists_amount = json_object_array_length(artists);
    tmp->album = calloc(tmp->albums_amount, sizeof(struct album));
    tmp->artist = calloc(tmp->artists_amount, sizeof(struct artist));

    if(title){
        size_t title_s = json_object_get_string_len(title);
        tmp->title = calloc(title_s + 1, sizeof(char));
        tmp->title = (char*)json_object_get_string(title);
    }
    tmp->id = json_object_get_int(id);

    for(k = 0; k < tmp->albums_amount; k++){
        json_object* album_item,* ali_name,* ali_id,* ali_coverUri,* ali_genre,* ali_year;
        album_item = json_object_array_get_idx(albums, k);
        ali_id = json_object_object_get(album_item, "id");
        ali_year = json_object_object_get(album_item, "year");
        ali_coverUri = json_object_object_get(album_item, "coverUri");
        ali_genre = json_object_object_get(album_item, "genre");
        ali_name = json_object_object_get(album_item, "title");

        if(ali_id){
            tmp->album[k].id = json_object_get_int(ali_id);
        }
        if(ali_year){
            tmp->album[k].year = json_object_get_int(ali_year);
        }
        if(ali_coverUri){
            tmp->album[k].coverUri = calloc(json_object_get_string_len(ali_coverUri) + 1, sizeof(char));
            tmp->album[k].coverUri = (char*)json_object_get_string(ali_coverUri);
        }
        if(ali_genre){
            tmp->album[k].genre = calloc(json_object_get_string_len(ali_genre) + 1, sizeof(char));
            tmp->album[k].genre = (char*)json_object_get_string(ali_genre);
        }
        if(ali_name){
            tmp->album[k].name = calloc(json_object_get_string_len(ali_name) + 1, sizeof(char));
            tmp->album[k].name = (char*)json_object_get_string(ali_name);
        }
    }

    for(j = 0; j < tmp->artists_amount; j++){
        json_object* artist_item,* ari_name,* ari_id;
        artist_item = json_object_array_get_idx(artists, j);
        ari_name = json_object_object_get(artist_item, "name");
        ari_id = json_object_object_get(artist_item, "id");
        if(ari_name){
            size_t artist_s = json_object_get_string_len(ari_name);
            tmp->artist[j].name = calloc(artist_s + 1, sizeof(char));
            tmp->artist[j].name = (char*)json_object_get_string(ari_name);
        }
        if(ari_id)tmp->artist[j].id = json_object_get_int(ari_id);
    }

    return tmp;
end:
    return NULL;
}

char* get_download_url(unsigned int trackId, userInfo* userinfo, char* proxy, char* proxy_type){
    response response;
    response.len = 0;
    response.data = NULL;
    char* url = calloc(75, sizeof(char));
    char* download_link = NULL;
    curl = curl_easy_init();
    struct curl_slist *headers = NULL;

    size_t tokenstrlen = 0;
    char* tokenstr = NULL;
    if(userinfo->access_token != NULL){
        tokenstrlen = strlen("Authorization: OAuth ") + strlen(userinfo->access_token) + 1;
        tokenstr = calloc(tokenstrlen, sizeof(char));
        snprintf(tokenstr, tokenstrlen, "Authorization: OAuth %s", userinfo->access_token);
    }
    if(curl){
        snprintf(url, 75, "%s%d%s", "https://api.music.yandex.net/tracks/", trackId, "/download-info");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
        printf("url: %s\n", url);
#endif
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows 10");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        headers = curl_slist_append(headers, "X-Yandex-Music-Client: WindowsPhone/4.20");
        if(tokenstr != NULL){
            headers = curl_slist_append(headers, tokenstr);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s, at get download info url\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        download* download_info = get_link(response);
#ifdef DEBUG
        printf("\n\n%s\n\n", response.data);
#endif
        if(download_info[0].downloadInfoUrl){
            free(url);
            free(response.data);
            response.len = 0;
            response.data = NULL;
            curl = curl_easy_init();
            if(curl){
                curl_easy_setopt(curl, CURLOPT_URL, download_info[0].downloadInfoUrl);
                if(proxy != NULL){
                    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
                    curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
                }
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
                curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
                curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows 10");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    fprintf(stderr, "curl_easy_perform() failed: %s, at get download info\n", curl_easy_strerror(res));
                }

                /* If returns 401 code, go to end. */
                CURLcode info;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &info);
                if(info == 401){
                    goto end;
                }

                curl_easy_cleanup(curl);
                char* sign = calloc(128, sizeof(char));
                char* startptr;
                startptr = strstr(download_info[0].downloadInfoUrl, "?sign=") + 6;
                snprintf(sign, 128, "%s", startptr);
                int p = 0;
                while(sign[p] != '&'){
                    p++;
                }
                sign[p] = '\0';

                char* host = calloc(40, sizeof(char));
                startptr = strstr(response.data, "<host>") + 6;
                snprintf(host, 40, "%s", startptr);
                p = 0;
                while(host[p] != '<'){
                    p++;
                }
                host[p] = '\0';

                char* path = calloc(512, sizeof(char));
                startptr = strstr(startptr, "<path>") + 6;
                snprintf(path, 512, "%s", startptr);
                p = 0;
                while(path[p] != '<'){
                    p++;
                }
                path[p] = '\0';

                char* ts = calloc(24, sizeof(char));
                startptr = strstr(startptr, "<ts>") + 4;
                snprintf(ts, 24, "%s", startptr);
                p = 0;
                while(ts[p] != '<'){
                    p++;
                }
                ts[p] = '\0';

                size_t link_s = 40 + strlen(host) + strlen(sign) + strlen(ts) + strlen(path);
                download_link = calloc(link_s, sizeof(char));
                snprintf(download_link, link_s, "%s%s%s%s%c%s%s%c", "https://", host, "/get-mp3/", sign, '/', ts, path, '\0');
end:

                return download_link;
            }else{goto end;}
        }else{goto end;}
    }
    goto end;
}

userInfo* get_token(char* grant_type, char* username, char* password, char* proxy, char* proxy_type){
    userInfo* token = calloc(1, sizeof(userInfo));
    char* client_id = "23cabbbdc6cd418abb4b39c32c41195d";
    char* client_secret = "53bc75238f0c4d08a118e51fe9203300";
    response response;
    response.len = 0;

    curl = curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, "https://oauth.yandex.ru/token");
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Yandex-music-client: Client");
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        size_t body_len = 1 + strlen("grant_type=&client_id=&client_secret=&username=&password=") + strlen(grant_type) + strlen(username) + strlen(password) + strlen(client_id) + strlen(client_secret);
        char* body = calloc(body_len, sizeof(char));
        snprintf(body, body_len, "grant_type=%s&client_id=%s&client_secret=%s&username=%s&password=%s", grant_type, client_id, client_secret, username, password);
        body[body_len] = '\0';

        char* content_lenght = malloc(25*sizeof(char));
        snprintf(content_lenght, sizeof(content_lenght), "content-lenght: %d", (int)body_len);
        headers = curl_slist_append(headers, content_lenght);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

        res = curl_easy_perform(curl);
        free(content_lenght);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s, at get_token()\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);

        json_object* JSON_p,* token_obj,* expires_obj,* token_type_obj,* uid_obj;

        JSON_p = json_tokener_parse(response.data);
        token_obj = json_object_object_get(JSON_p, "access_token");
        expires_obj = json_object_object_get(JSON_p, "expires_in");
        token_type_obj = json_object_object_get(JSON_p, "token_type");
        uid_obj = json_object_object_get(JSON_p, "uid");

        token->access_token = calloc(json_object_get_string_len(token_obj), sizeof(char));
        token->access_token = (char*)json_object_get_string(token_obj);

        token->expires_in = json_object_get_int(expires_obj);

        token->token_type = calloc(json_object_get_string_len(token_type_obj), sizeof(char));
        token->token_type = (char*)json_object_get_string(token_type_obj);

        token->uid = json_object_get_int(uid_obj);

        //free(response.data);
        free(body);
        return token;
    }
    return NULL;
}
int download_track(const char* name, const char* url, char* proxy, char* proxy_type) {
    printf("url: %s\n track name: %s\n", url, name);
    FILE *fp;
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(name,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            curl_easy_cleanup(curl);
            curl = NULL;
            fclose(fp);            return -1;
        }
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    return 0;
}

tracks* get_likedtracklist(unsigned int uuid, userInfo* userinfo, char* proxy, char* proxy_type){
    response response;
    struct tracks* tracks_info = NULL;
    response.len = 0;
    response.data = NULL;
    curl = curl_easy_init();
    if(curl){
        char* url = calloc(150, sizeof(char));
        snprintf(url, 150, "%s%d%s", "https://api.music.yandex.net/users/", uuid, "/likes/tracks");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        if(proxy != NULL){
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, proxy_type);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_CAINFO, "crt\\cacert.pem");
        curl_easy_setopt(curl, CURLOPT_CAPATH, "crt\\cacert.pem");
#endif
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
#endif
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows 10");
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
        //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "X-Yandex-Music-Client: WindowsPhone/4.20");

        size_t tokenstrlen = 0;
        char* tokenstr = NULL;
        if(userinfo->access_token != NULL){
            tokenstrlen = strlen("Authorization: OAuth ") + strlen(userinfo->access_token) + 1;

            tokenstr = calloc(tokenstrlen, sizeof(char));
            snprintf(tokenstr, tokenstrlen, "Authorization: OAuth %s", userinfo->access_token);
            headers = curl_slist_append(headers, tokenstr);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s, at yam_search()\n", curl_easy_strerror(res));
        }

        free(url);
        if(!response.data){goto end;}

        int status;
        struct json_object* json_resp,* result,* library,* tracks;
        json_resp = json_tokener_parse(response.data);
        json_object_object_get_ex(json_resp, "result", &result);
        json_object_object_get_ex(result, "library", &library);
        status = json_object_object_get_ex(library, "tracks", &tracks);
        if(status == 0){
            printf("\n\nError:\t%s\n\n", response.data);
            goto end;
        }
        curl_easy_cleanup(curl);
        tracks_info = get_likedtracks_info(tracks, userinfo, proxy, proxy_type);
    }

end:
    free(response.data);
    return tracks_info;
}



static tracks* get_likedtracks_info(json_object* input_info, userInfo* userinfo, char* proxy, char* proxy_type){
    size_t trackItm_s = json_object_array_length(input_info);
    struct tracks* tmp = malloc(sizeof(tracks)*trackItm_s);
    tmp->item = calloc(trackItm_s, sizeof(struct track));
    tmp->tracks_col = trackItm_s;

    uint i;
    tmp->item = calloc(tmp->tracks_col, sizeof(track));
    for(i = 0; i < tmp->tracks_col; i++){
        json_object* track,* id;
        track = json_object_array_get_idx(input_info, i);
        id = json_object_object_get(track, "id");
        tmp->item[i] = *get_track_info_from_id(json_object_get_int(id), userinfo, proxy, proxy_type);
        uint tmpid = json_object_get_int(id);
        tmp->item[i].id = malloc(sizeof(tmpid));
        tmp->item[i].id = tmpid;
    }
    return tmp;
}
