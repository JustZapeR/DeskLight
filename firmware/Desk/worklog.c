#include "worklog.h"
#include <string.h>

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put(uint8_t *p, uint32_t x)
{
    for(unsigned k=0;k<4;k++) p[k]=(uint8_t)(x>>(8*k));
}
uint32_t worklog_crc(const uint8_t *p, size_t n)
{
    uint32_t crc=0xffffffffu;
    while(n--) {
        crc^=*p++;
        for(unsigned k=0;k<8;k++) crc=(crc>>1)^((crc&1) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
static bool blank(const uint8_t *p)
{
    for(unsigned k=0;k<WORK_RECORD_BYTES;k++) if(p[k]!=255) return false;
    return true;
}
static bool valid(const uint8_t *p)
{
    return word(p)==0x334b5344u && word(p+28)==0x54494d43u
        && word(p+24)==worklog_crc(p,24);
}
void worklog_open(worklog *log, work_store store)
{
    memset(log,0,sizeof *log); log->store=store; log->latest=-1;
    uint8_t bytes[WORK_RECORD_BYTES];
    for(unsigned slot=0;slot<WORK_RECORDS;slot++) {
        store.read(store.context,slot*WORK_RECORD_BYTES,bytes,sizeof bytes);
        if(blank(bytes)) continue;
        if(!valid(bytes)) { log->damaged++; continue; }
        log->accepted++;
        uint32_t seq=word(bytes+4);
        if(log->latest<0 || ((seq-log->sequence)!=0 && (seq-log->sequence)<0x80000000u)) {
            log->latest=(int)slot; log->sequence=seq;
            log->totals=(work_totals){word(bytes+8),word(bytes+12),word(bytes+16),word(bytes+20)};
        }
    }
}
bool worklog_save(worklog *log, const work_totals *totals)
{
    uint8_t bytes[WORK_RECORD_BYTES];
    unsigned page=log->latest<0 ? 0u : (unsigned)log->latest/64;
    unsigned begin=log->latest<0 ? 0u : (unsigned)log->latest%64+1;
    unsigned slot=128;
    for(unsigned k=begin;k<64;k++) {
        log->store.read(log->store.context,(page*64+k)*32,bytes,sizeof bytes);
        if(blank(bytes)) { slot=page*64+k; break; }
    }
    if(slot==128) {
        page^=1;
        /* The page with the newest committed checkpoint is never erased. */
        if(!log->store.erase_page(log->store.context,page)) return false;
        slot=page*64;
    }
    put(bytes,0x334b5344u); put(bytes+4,log->sequence+1);
    put(bytes+8,totals->elapsed_ms); put(bytes+12,totals->active_ms);
    put(bytes+16,totals->dim_work_ms); put(bytes+20,totals->tilt_entries);
    put(bytes+24,worklog_crc(bytes,24)); put(bytes+28,0x54494d43u);
    for(unsigned k=0;k<32;k+=8)
        if(!log->store.program8(log->store.context,slot*32+k,bytes+k)) return false;
    uint8_t check[WORK_RECORD_BYTES];
    log->store.read(log->store.context,slot*32,check,sizeof check);
    if(memcmp(bytes,check,sizeof check) || !valid(check)) return false;
    log->latest=(int)slot; log->sequence++; log->totals=*totals; log->accepted++;
    return true;
}
