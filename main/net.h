#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "settings.h"
#include "ui.h"

#define WB_VERSION "1.1.0"

void net_start(wb_settings_t *s);
void net_ui_action(ui_action_t a, void *user);  /* UI button callback */
bool net_local_time(struct tm *out);            /* local clock at the weather location */

/* Shared with portal.c */
bool net_http_get(const char *url, char **body, size_t *len);
void portal_run(wb_settings_t *s);  /* never returns: restarts after the form is saved */
