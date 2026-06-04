#ifndef HLS_MANAGER_H
#define HLS_MANAGER_H

#include "site.h"

// Intercepts the request. Returns 1 if handled (HLS request), 0 if normal file.
int handle_hls_request(int client_fd,
                       Header* header,
                       const char* abs_path,
                       const char* content_type_str);

void init_hls_janitor();

#endif    // HLS_MANAGER_H