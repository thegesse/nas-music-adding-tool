#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <stdbool.h>

#include "../scrapper/scrapper.h"
#include "handler.h"



void apply_cors_headers(struct MHD_Connection *connection, struct MHD_Response *response) {
    const char *origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");

    if (origin != NULL) {
        for (int i = 0; ALLOWED_ORIGINS[i] != NULL; i++) {
            if (strcmp(origin, ALLOWED_ORIGINS[i]) == 0) {
                MHD_add_response_header(response, "Access-Control-Allow-Origin", origin);
                break;
            }
        }
    }

    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, PATCH");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
}

bool validate_route(const char *url, const char *prefix) {
    return strncmp(url, prefix, strlen(prefix)) == 0;
}

char *simple_message(const char *msg) {
    char *buffer = malloc(strlen(msg) + 16); // Space for JSON wrapper
    snprintf(buffer, strlen(msg) + 16, "{\"message\":\"%s\"}", msg);
    return buffer;
}

struct MHD_Response *HTTP_build_response_JSON(const char *json_body) {
    if (!json_body) return NULL;
    return MHD_create_response_from_buffer(strlen(json_body), (void *)json_body, MHD_RESPMEM_MUST_COPY);
}


HTTP_response handle_search_track_route(const char *query) {
    if (!query || strlen(query) == 0) {
        return (HTTP_response){ .body = strdup("{\"error\":\"Empty query\"}"), .status = 400 };
    }

    FILE *temp_file = tmpfile();
    if (!temp_file) {
        return (HTTP_response){ .body = strdup("{\"error\":\"Failed to create temporary file\"}"), .status = 500 };
    }

    int result = SearchJamendo(query, temp_file);
    if (result != 0) {
        fclose(temp_file);
        return (HTTP_response){ .body = strdup("{\"error\":\"Failed to search Jamendo\"}"), .status = 500 };
    }

    fseek(temp_file, 0, SEEK_END);
    long file_size = ftell(temp_file);
    fseek(temp_file, 0, SEEK_SET);

    char *csv_data = malloc(file_size + 1);
    if (!csv_data) {
        fclose(temp_file);
        return (HTTP_response){ .body = strdup("{\"error\":\"Memory allocation failed\"}"), .status = 500 };
    }

    fread(csv_data, 1, file_size, temp_file);
    csv_data[file_size] = '\0';
    fclose(temp_file);

    return (HTTP_response){ .body = csv_data, .status = 200 };
}


