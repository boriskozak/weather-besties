/* LVGL allocator cores (iter-35): LVGL's object graph lives in PSRAM.
 *
 * lv_conf.h sets LV_USE_STDLIB_MALLOC to LV_STDLIB_CUSTOM, so these
 * cores (plus the init/pool stubs LVGL links against) are the whole
 * LVGL allocator. Before iter-35 LVGL used the C library malloc, and
 * CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096 parked every allocation
 * of 4 KB or less in internal DRAM: hundreds of small objects
 * (labels, styles, the My Day card tree) competing with the WiFi
 * and TLS buffers for the scarcest memory on the board. The
 * fd2f37c build's post-fetch log read 43 KB internal free.
 *
 * Allocations go to PSRAM first (16-byte aligned, like malloc),
 * falling back to internal RAM only when PSRAM cannot serve them,
 * so an exhausted PSRAM degrades to the pre-iter-35 behaviour
 * instead of failing the UI. realloc is malloc + copy + free rather
 * than heap_caps_realloc: the old size comes from
 * heap_caps_get_allocated_size, which is defined for every
 * heap_caps allocation, aligned or not.
 *
 * PSRAM is up before app_main runs (it is initialised by the
 * startup code), and lv_init happens inside board_init, so no LVGL
 * allocation can precede the PSRAM heap.
 */
#include <string.h>

#include "esp_heap_caps.h"
#include "lvgl.h"

void lv_mem_init(void)
{
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    (void)mem;
    (void)bytes;
    return NULL;   /* single heap, no pools */
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    (void)pool;
}

void *lv_malloc_core(size_t size)
{
    void *p = heap_caps_aligned_alloc(16, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_aligned_alloc(16, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return p;
}

void *lv_realloc_core(void *p, size_t new_size)
{
    if (!p) {
        return lv_malloc_core(new_size);
    }
    if (new_size == 0) {
        lv_free_core(p);
        return NULL;
    }
    void *np = lv_malloc_core(new_size);
    if (!np) {
        return NULL;
    }
    size_t old = heap_caps_get_allocated_size(p);
    memcpy(np, p, old < new_size ? old : new_size);
    lv_free_core(p);
    return np;
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    memset(mon_p, 0, sizeof(*mon_p));
    mon_p->total_size = info.total_allocated_bytes + info.total_free_bytes;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->used_cnt = info.allocated_blocks;
    mon_p->max_used = info.total_allocated_bytes;
    if (mon_p->total_size) {
        mon_p->used_pct = (uint8_t)(info.total_allocated_bytes * 100 / mon_p->total_size);
    }
    if (info.total_free_bytes) {
        mon_p->frag_pct = (uint8_t)(100 - (size_t)info.largest_free_block * 100 / info.total_free_bytes);
    }
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
