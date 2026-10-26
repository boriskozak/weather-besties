/*
 * Over-the-air updates. The board checks firmware/latest.json in the repo (see
 * components/wb_ui/update.h), downloads a newer app into the other app slot,
 * checks its SHA-256, and restarts into it. The new version runs "on trial": it
 * has to fetch the weather within OTA_TRIAL_MIN minutes (ota_mark_good), or the
 * board restarts and the bootloader goes back to the previous version.
 */
#pragma once

#include <stdbool.h>

#define OTA_TRIAL_MIN 10

/* At boot: starts the trial timer if this version is new and not yet proven. */
void ota_boot_check(void);

/* After a weather download has worked: this version is good, keep it. */
void ota_mark_good(void);

/* Checks for a newer version and installs it (restarting into it). Returns if there was
 * nothing to install or it failed. manual: show the result on screen even when it's
 * "already up to date" or an error. */
void ota_run(bool manual);
