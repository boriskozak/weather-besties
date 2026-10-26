#include "update.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static bool parse_version(const char *s, int v[3])
{
    v[0] = v[1] = v[2] = 0;
    return s && sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3 && v[0] >= 0 && v[1] >= 0 && v[2] >= 0;
}

int update_version_cmp(const char *a, const char *b)
{
    int va[3], vb[3];
    parse_version(a, va);
    parse_version(b, vb);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i]) {
            return va[i] < vb[i] ? -1 : 1;
        }
    }
    return 0;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

bool update_parse_manifest(const char *json, size_t len, wb_manifest_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) {
        return false;
    }
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *app = cJSON_GetObjectItemCaseSensitive(root, "app");
    const cJSON *sha = cJSON_GetObjectItemCaseSensitive(root, "sha256");
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "size");
    int ver[3];
    bool ok = cJSON_IsString(v) && parse_version(v->valuestring, ver) && strlen(v->valuestring) < sizeof(out->version) &&
              cJSON_IsString(app) && app->valuestring[0] && strlen(app->valuestring) < sizeof(out->app) &&
              !strchr(app->valuestring, '/') && !strstr(app->valuestring, "..") &&
              cJSON_IsString(sha) && strlen(sha->valuestring) == 64 &&
              cJSON_IsNumber(size) && size->valuedouble > 0 && size->valuedouble < 16.0 * 1024 * 1024;
    for (int i = 0; ok && i < 32; i++) {
        int hi = hexval(sha->valuestring[2 * i]), lo = hexval(sha->valuestring[2 * i + 1]);
        ok = hi >= 0 && lo >= 0;
        out->sha256[i] = (uint8_t)(hi << 4 | lo);
    }
    if (ok) {
        snprintf(out->version, sizeof(out->version), "%s", v->valuestring);
        snprintf(out->app, sizeof(out->app), "%s", app->valuestring);
        out->size = (uint32_t)size->valuedouble;
    }
    cJSON_Delete(root);
    return ok;
}
