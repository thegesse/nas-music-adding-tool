#include <stdio.h>
#include <string.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "Api/handler.h"
#include "Api/utils.h"

#define PORT 8080

typedef struct {
    char *data;
    size_t size;
} RequestBuffer;

enum MHD_Result default_handler(
    void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {


    RequestBuffer *buf = (RequestBuffer *)*con_cls;

    if (*upload_data_size > 0) {
        char *new_data = realloc(buf->data, buf->size + *upload_data_size + 1);
        if (!new_data) return MHD_NO;

        buf->data = new_data;
        memcpy(buf->data + buf->size, upload_data, *upload_data_size);
        buf->size += *upload_data_size;
        buf->data[buf->size] = '\0';

        *upload_data_size = 0;
        return MHD_YES;
    }


    char *payload = buf->data ? buf->data : "";

    char *url_str = (char *)url;
    char *method_str = (char *)method;
    int ret;

    struct MHD_Response *response;
    HTTP_response response_api;

    log_api(url_str, method_str);

    if (strcmp(method_str, "OPTIONS") == 0) {
        response_api = (HTTP_response){ .body = strdup("{}"), .status = 200 };
    } else if (strcmp(url_str, "/") == 0 || validate_route(url_str, "/users")) {
        response_api = user_router(conn, url_str, method_str, payload);
    } else if (validate_route(url_str, "/tasks")) {
        response_api = task_router(conn, url_str, method_str, payload);
    } else {
        response_api = (HTTP_response){
            .body = simple_message("not found"),
            .status = MHD_HTTP_NOT_FOUND,
        };
    }

    if (buf->data) free(buf->data);
    free(buf);
    *con_cls = NULL;

    response = HTTP_build_response_JSON(response_api.body);

    if (!response) {
        if (response_api.body) free(response_api.body);
        return MHD_NO;
    }

    apply_cors_headers(connection, response);
    ret = MHD_queue_response(connection, response_api.status, response);

    MHD_destroy_response(response);
    if (response_api.body) free(response_api.body);

    return ret;
}

static volatile sig_atomic_t running = 1;

void handle_signal(int signal) {
    (void) signal;
    running = 0;
}
int main() {

    printf("connecting to db");
    PGconn *conn = connect_to_db();
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to db failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return 1;
    }

    printf("starting server on port %d\n", PORT);

    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, PORT, NULL, NULL, &default_handler, conn, MHD_OPTION_END);

    if (!daemon) {
        PQfinish(conn);
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT) failed");
        MHD_stop_daemon(daemon);
        PQfinish(conn);
        return 1;
    }

    while (running) {
        sleep(1);
    }

    printf("\nShutting down server\n");

    MHD_stop_daemon(daemon);
    PQfinish(conn);
    printf("Server stopped\n");
    return 0;
}