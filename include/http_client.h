#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>

typedef struct {
    int curl_rc;
    long http_code;
    double total_ms;
    double connect_ms;
    double ttfb_ms;
    size_t bytes;
} HttpMetricas;

int http_init(void);
void http_cleanup(void);
char *http_get_ex(const char *url, HttpMetricas *m);
char *http_get(const char *url);

#endif
