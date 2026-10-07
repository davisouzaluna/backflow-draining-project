#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

int http_init(void);
void http_cleanup(void);
char *http_get(const char *url);

#endif
