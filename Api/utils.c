#include "utils.h"

#include <stdio.h>

void log_api(const char *url, const char *method) {
    printf("[API] %s %s\n", method, url);
}