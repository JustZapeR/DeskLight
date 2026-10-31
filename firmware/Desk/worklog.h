#ifndef DESKLIGHT_WORKLOG_H
#define DESKLIGHT_WORKLOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Checkpoints describe observed work, never occupancy or an alarm. */
typedef struct {
    uint32_t elapsed_ms, active_ms, dim_work_ms, tilt_entries;
} work_totals;
typedef struct {
    void *context;
    void (*read)(void *, unsigned, void *, size_t);
    bool (*program8)(void *, unsigned, const uint8_t *);
    bool (*erase_page)(void *, unsigned);
} work_store;
typedef struct {
    work_store store;
    work_totals totals;
    uint32_t sequence, accepted, damaged;
    int latest;
} worklog;
enum { WORK_PAGE_BYTES=2048, WORK_RECORD_BYTES=32, WORK_RECORDS=128 };
void worklog_open(worklog *log, work_store store);
bool worklog_save(worklog *log, const work_totals *totals);
uint32_t worklog_crc(const uint8_t *bytes, size_t count);
#endif
