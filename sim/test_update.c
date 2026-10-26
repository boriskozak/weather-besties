/* Tests for components/wb_ui/update.c (the OTA manifest and version checks). Run: make test */
#include <stdio.h>
#include <string.h>

#include "update.h"

static int fails;
#define CHECK(cond, msg)                                   \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            fails++;                                       \
        }                                                  \
    } while (0)

#define SHA "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdef"

static bool parse(const char *j, wb_manifest_t *m)
{
    return update_parse_manifest(j, strlen(j), m);
}

int main(void)
{
    CHECK(update_version_cmp("1.4.3", "1.5.0") < 0, "1.4.3 < 1.5.0");
    CHECK(update_version_cmp("1.10.0", "1.9.9") > 0, "1.10.0 > 1.9.9 (numbers, not text)");
    CHECK(update_version_cmp("2.0.0", "2.0.0") == 0, "equal");
    CHECK(update_version_cmp("garbage", "0.0.1") < 0, "garbage counts as 0.0.0");

    wb_manifest_t m;
    CHECK(parse("{\"version\":\"1.5.0\",\"app\":\"weather-besties-v1.5.0-app.bin\",\"sha256\":\"" SHA "\",\"size\":3566176}", &m),
          "good manifest");
    CHECK(!strcmp(m.version, "1.5.0") && m.size == 3566176 && m.sha256[0] == 0x01 && m.sha256[31] == 0xef &&
              m.sha256[16] == 0x01 && m.sha256[23] == 0xef,
          "fields and hash bytes (mixed-case hex)");
    CHECK(!parse("{\"version\":\"1.5\",\"app\":\"a.bin\",\"sha256\":\"" SHA "\",\"size\":10}", &m), "bad version");
    CHECK(!parse("{\"version\":\"1.5.0\",\"app\":\"../a.bin\",\"sha256\":\"" SHA "\",\"size\":10}", &m), "path in app");
    CHECK(!parse("{\"version\":\"1.5.0\",\"app\":\"https://evil/a.bin\",\"sha256\":\"" SHA "\",\"size\":10}", &m),
          "url in app");
    CHECK(!parse("{\"version\":\"1.5.0\",\"app\":\"a.bin\",\"sha256\":\"abc\",\"size\":10}", &m), "short hash");
    CHECK(!parse("{\"version\":\"1.5.0\",\"app\":\"a.bin\",\"sha256\":\"" SHA "\"}", &m), "no size");
    CHECK(!parse("not json", &m), "not json");

    printf(fails ? "%d FAILED\n" : "all update tests passed\n", fails);
    return fails != 0;
}
