/*
 * Over-the-air updates: the manifest the board checks (firmware/latest.json in the
 * repo) and version comparison. Plain C, so sim/test_update.c tests the same code.
 *
 * latest.json:
 *   {"version": "2.1.0", "app": "weather-besties-v2.1.0-app.bin",
 *    "sha256": "<64 hex>", "size": 3566176}
 * "app" is a file name next to latest.json. Only versions newer than the running
 * one are installed, so publishing an older manifest never downgrades a board.
 *
 * Ported from Michael Lippy's fork (iter-32); the only change is the repo the
 * manifest is fetched from: ours, not his.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where the board looks. Raw files from the repo's main branch (public repo). */
#define WB_UPDATE_BASE "https://raw.githubusercontent.com/boriskozak/weather-besties/main/firmware/"
#define WB_UPDATE_MANIFEST WB_UPDATE_BASE "latest.json"

typedef struct {
    char version[16];
    char app[96];
    uint8_t sha256[32];
    uint32_t size;
} wb_manifest_t;

/* False if anything is missing or malformed (bad version, file name with a slash, bad hash). */
bool update_parse_manifest(const char *json, size_t len, wb_manifest_t *out);

/* Compares "1.4.3" style versions: <0 if a is older than b, 0 if equal, >0 if newer.
 * Unparseable versions count as 0.0.0. */
int update_version_cmp(const char *a, const char *b);

#ifdef __cplusplus
}
#endif
