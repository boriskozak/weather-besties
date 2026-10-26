#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "settings.h"
#include "ui.h"

#define WB_VERSION "2.0.1"

void net_start(wb_settings_t *s);
void net_ui_action(ui_action_t a, void *user);  /* UI button callback */
bool net_local_time(struct tm *out);            /* local clock at the weather location */

/* Shared with portal.c */
bool net_http_get(const char *url, char **body, size_t *len);
/* Streams a (possibly big) download to cb a chunk at a time, following redirects.
 * cb returns false to stop early (then this returns false). Used by ota.c. */
bool net_http_stream(const char *url, bool (*cb)(const char *, size_t, void *), void *ctx, int *status_out,
                     size_t *total);
void portal_run(wb_settings_t *s);  /* never returns: restarts after the form is saved */
