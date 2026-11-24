#ifndef HANDLER_H
#define HANDLER_H
    typedef struct {
    char *body;
    int status;
    } HTTP_response;

    const char *ALLOWED_ORIGINS[] = {
    "http://localhost:3000"
    };

    HTTP_response handle_search_track_route(const char *query);

    void apply_cors_headers(struct MHD_Connection *connection, struct MHD_Response *response);
    bool validate_route(const char *url, const char *prefix);
    char *simple_message(const char *msg);
    struct MHD_Response *HTTP_build_response_JSON(const char *json_body);
#endif