#include "scrapper.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t size;
} ResponseBuffer;

static size_t WriteCallback(void *contents, size_t size, size_t count, void *userp) {
    size_t bytes = size * count;
    ResponseBuffer *response = userp;
    char *resized = realloc(response->data, response->size + bytes + 1);

    if (!resized) {
        return 0;
    }

    response->data = resized;
    memcpy(response->data + response->size, contents, bytes);
    response->size += bytes;
    response->data[response->size] = '\0';
    return bytes;
}

static const char *GetString(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return "";
    }
    return json_object_get_string(value);
}

//reminder to skip this
static int IsDownloadAllowed(struct json_object *track) {
    struct json_object *allowed = NULL;
    return json_object_object_get_ex(track, "audiodownload_allowed", &allowed) &&
           json_object_get_boolean(allowed);
}

static void WriteCSVField(FILE *output, const char *value) {
    fputc('"', output);
    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        if (*cursor == '"') {
            fputc('"', output);
        }
        fputc(*cursor, output);
    }
    fputc('"', output);
}

int SearchJamendo(const char *query, FILE *output) {
    const char *client_id = getenv("JAMENDO_CLIENT_ID");
    if (!client_id || !*client_id) {
        fprintf(stderr, "Set JAMENDO_CLIENT_ID to your Jamendo API client ID.\n");
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Could not initialize libcurl.\n");
        return -1;
    }

    char *escaped_query = curl_easy_escape(curl, query, 0);
    char *escaped_client_id = curl_easy_escape(curl, client_id, 0);
    if (!escaped_query || !escaped_client_id) {
        fprintf(stderr, "Could not encode the Jamendo search request.\n");
        curl_free(escaped_query);
        curl_free(escaped_client_id);
        curl_easy_cleanup(curl);
        return -1;
    }

    char url[4096];
    int url_length = snprintf(
        url, sizeof(url), //Change url to something else
        "https://api.jamendo.com/v3.0/tracks/?client_id=%s&format=json&limit=20&search=%s&audiodlformat=mp32&include=licenses&type=single+albumtrack",
        escaped_client_id, escaped_query);
    curl_free(escaped_query);
    curl_free(escaped_client_id);
    if (url_length < 0 || (size_t)url_length >= sizeof(url)) {
        fprintf(stderr, "Search query is too long.\n");
        curl_easy_cleanup(curl);
        return -1;
    }

    ResponseBuffer response = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NAS-Music-Adding-Tool/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || status < 200 || status >= 300 || !response.data) {
        fprintf(stderr, "Jamendo request failed: %s (HTTP %ld).\n",
                result == CURLE_OK ? "unexpected response" : curl_easy_strerror(result), status);
        free(response.data);
        return -1;
    }

    struct json_object *document = json_tokener_parse(response.data);
    free(response.data);
    if (!document) {
        fprintf(stderr, "Jamendo returned invalid JSON.\n");
        return -1;
    }

    struct json_object *results = NULL;
    if (!json_object_object_get_ex(document, "results", &results) ||
        !json_object_is_type(results, json_type_array)) {
        fprintf(stderr, "Jamendo response did not contain a results list.\n");
        json_object_put(document);
        return -1;
    }

    fputs("title,artist,album,mp3_download_url,license_url,track_url\n", output);
    int written = 0;
    size_t result_count = json_object_array_length(results);
    for (size_t index = 0; index < result_count; index++) {
        struct json_object *track = json_object_array_get_idx(results, index);
        if (!IsDownloadAllowed(track) || !*GetString(track, "audiodownload")) {
            continue;
        }

        const char *fields[] = {
            GetString(track, "name"),
            GetString(track, "artist_name"),
            GetString(track, "album_name"),
            GetString(track, "audiodownload"),
            GetString(track, "license_ccurl"),
            GetString(track, "shareurl")
        };
        for (size_t field = 0; field < sizeof(fields) / sizeof(fields[0]); field++) {
            if (field > 0) {
                fputc(',', output);
            }
            WriteCSVField(output, fields[field]);
        }
        fputc('\n', output);
        written++;
    }

    json_object_put(document);
    return written;
}