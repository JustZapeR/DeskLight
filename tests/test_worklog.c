#include "../firmware/Desk/worklog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned char bytes[4096]; int budget; unsigned erases; } flash_model;
static void read_bytes(void *ctx,unsigned at,void *out,size_t n)
{ flash_model *f=ctx; assert(at+n<=sizeof f->bytes); memcpy(out,f->bytes+at,n); }
static bool program(void *ctx,unsigned at,const uint8_t *p)
{
    flash_model *f=ctx; assert(at%8==0 && at+8<=sizeof f->bytes);
    for(unsigned n=0;n<8;n++) {
        if(f->budget==0) return false;
        assert((f->bytes[at+n]&p[n])==p[n]); f->bytes[at+n]&=p[n];
        if(f->budget>0) f->budget--;
    }
    return true;
}
static bool erase(void *ctx,unsigned page)
{
    flash_model *f=ctx; assert(page<2); f->erases++;
    for(unsigned n=page*2048;n<(page+1)*2048;n++) {
        if(f->budget==0) return false;
        f->bytes[n]=255; if(f->budget>0) f->budget--;
    }
    return true;
}
static work_store storage(flash_model *f) { return (work_store){f,read_bytes,program,erase}; }
static void fresh(flash_model *f) { memset(f,255,sizeof *f); f->budget=-1; f->erases=0; }
int main(void)
{
    flash_model f,backup; worklog log,reboot; fresh(&f);
    assert(worklog_crc((const uint8_t *)"123456789",9)==0xcbf43926u);
    worklog_open(&log,storage(&f)); assert(log.latest==-1 && log.damaged==0);
    work_totals a={60000,42000,12000,2},b={120000,84000,20000,3};
    assert(worklog_save(&log,&a)); backup=f;
    for(int cut=0;cut<=32;cut++) {
        f=backup; f.budget=cut; worklog_open(&log,storage(&f));
        bool ok=worklog_save(&log,&b);
        worklog_open(&reboot,storage(&f));
        assert(reboot.sequence==(cut<32 ? 1u : 2u));
        assert(reboot.totals.active_ms==(cut<32 ? a.active_ms : b.active_ms));
        assert(ok==(cut==32));
        f.budget=-1; assert(worklog_save(&reboot,&b));
    }
    f=backup; f.bytes[12]^=1; worklog_open(&reboot,storage(&f));
    assert(reboot.latest==-1 && reboot.damaged==1);
    fresh(&f); worklog_open(&log,storage(&f));
    for(unsigned n=1;n<=192;n++) { a.elapsed_ms=n*60000; assert(worklog_save(&log,&a)); }
    assert(f.erases==2); backup=f;
    /* At rollover, interrupt every byte of inactive-page erase and new record. */
    for(int cut=0;cut<=2080;cut++) {
        f=backup; f.budget=cut; worklog_open(&log,storage(&f));
        (void)worklog_save(&log,&b); worklog_open(&reboot,storage(&f));
        assert(reboot.sequence==(cut<2080 ? 192u : 193u));
    }
    fresh(&f); worklog_open(&log,storage(&f)); log.sequence=0xfffffffeu;
    assert(worklog_save(&log,&a)); assert(log.sequence==0xffffffffu);
    assert(worklog_save(&log,&b)); worklog_open(&reboot,storage(&f));
    assert(reboot.sequence==0 && reboot.totals.active_ms==b.active_ms);
    printf("worklog PASS: CRC vector, 33 record cuts, 2081 rollover cuts, corruption, 192 saves, sequence wrap\n");
    return 0;
}
