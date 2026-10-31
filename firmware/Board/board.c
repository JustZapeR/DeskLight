#include "board.h"
#include "../Desk/console.h"
#include "../Sensors/imu.h"
#include "../Desk/worklog.h"
#include "measure.h"
#include "build_id.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
static ADC_HandleTypeDef adc;
static desk state;
static desk_console console;
static desk_led led;
static uint8_t tx[2048], rx[128];
static uint32_t rx_time[128];
static unsigned tx_head, tx_tail;
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_overflow;
static volatile uint32_t rx_dropped, uart_overrun, event_us, event_coalesced;
static volatile bool event_pending;
static uint32_t tx_dropped, rx_peak, tx_peak, init_due, status_due, history_due, save_due;
static uint32_t sensor_period=100000, metric_epoch;
static task_meter meter[10];
static uint64_t periodic_flash_sum;
enum { M_PIR, M_ADC, M_IMU, M_CONTROL, M_COMMAND, M_EXPORT, M_FLASH, M_LOOP, M_REACTION, M_INIT };
static const char *const meter_names[]={"pir","adc","imu","control","command","export","flash","loop","pir_latency","init"};
typedef struct { uint32_t ms, active_ms, dim_ms; uint16_t avg, angle10; } history_row;
static history_row history[64];
static unsigned history_count, history_head;
static worklog journal;
static work_totals totals;
static uint32_t accounting_tick;
static bool was_tilt;
extern UART_HandleTypeDef huart2;
extern uint32_t desk_heap_bytes(void);
static bool adc_ready, imu_ready, started;
static bool forced_imu, forced_adc, paused_imu, paused_adc;
static unsigned previous_flags;

static void emit(const char *text)
{
    size_t n=strlen(text);
    if (n >= sizeof tx - ((tx_head-tx_tail)&2047u)) { tx_dropped++; return; }
    while(*text) { tx[tx_head]=*text++; tx_head=(tx_head+1)&2047u; }
    unsigned used=(tx_head-tx_tail)&2047u;
    if(used>tx_peak) tx_peak=used;
}

void USART2_IRQHandler(void)
{
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t byte=(uint8_t)USART2->RDR;
        unsigned next=(rx_head+1)&127u;
        if(next!=rx_tail) { rx[rx_head]=byte; rx_time[rx_head]=measure_us(); rx_head=next; }
        else { rx_overflow=true; rx_dropped++; }
    }
    if(USART2->ISR & USART_ISR_ORE) uart_overrun++;
    USART2->ICR=USART_ICR_ORECF;
}

void EXTI0_1_IRQHandler(void)
{
    if(EXTI->RPR1 & 1u) {
        EXTI->RPR1=1u;
        if(event_pending) event_coalesced++;
        else { event_us=measure_us(); event_pending=true; }
    }
}

static void flash_read(void *ctx,unsigned offset,void *out,size_t count)
{
    (void)ctx; memcpy(out,(const void *)(0x08007000u+offset),count);
}
static bool flash_program(void *ctx,unsigned offset,const uint8_t *bytes)
{
    (void)ctx; uint64_t value; memcpy(&value,bytes,8);
    if(HAL_FLASH_Unlock()!=HAL_OK) return false;
    bool ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,0x08007000u+offset,value)==HAL_OK;
    HAL_FLASH_Lock(); return ok;
}
static bool flash_erase(void *ctx,unsigned page)
{
    (void)ctx;
    FLASH_EraseInitTypeDef erase={0}; uint32_t error;
    erase.TypeErase=FLASH_TYPEERASE_PAGES; erase.Page=14+page; erase.NbPages=1;
    if(HAL_FLASH_Unlock()!=HAL_OK) return false;
    bool ok=HAL_FLASHEx_Erase(&erase,&error)==HAL_OK;
    HAL_FLASH_Lock();
    if(ok) {
        const volatile uint32_t *words=(const volatile uint32_t *)(0x08007000u+page*2048u);
        for(unsigned i=0;i<512;i++) if(words[i]!=0xffffffffu) return false;
    }
    return ok;
}
static void account(uint32_t now)
{
    uint32_t dt=now-accounting_tick; accounting_tick=now;
    totals.elapsed_ms+=dt;
    if(state.mode==DESK_ACTIVE) totals.active_ms+=dt;
    if(state.need_light) totals.dim_work_ms+=dt;
    if(state.tilt && !was_tilt) totals.tilt_entries++;
    was_tilt=state.tilt;
}
static void checkpoint(void)
{
    uint32_t began=measure_us();
    account(HAL_GetTick());
    GPIOA->BSRR=GPIO_PIN_7;
    bool ok=worklog_save(&journal,&totals);
    GPIOA->BSRR=(uint32_t)GPIO_PIN_7<<16;
    uint32_t elapsed=measure_us()-began;
    measure_add(&meter[M_FLASH],elapsed);
    char text[160];
    snprintf(text,sizeof text,"SAVE t_us=%lu result=%s seq=%lu elapsed_us=%lu hal_error=%lu\r\n",
        (unsigned long)began,ok ? "OK" : "ERROR",(unsigned long)journal.sequence,
        (unsigned long)elapsed,(unsigned long)HAL_FLASH_GetError()); emit(text);
}
static void metrics_reset(void)
{
    memset(meter,0,sizeof meter); periodic_flash_sum=0; metric_epoch=measure_us();
    meter[M_PIR].due=metric_epoch+20000;
    meter[M_ADC].due=meter[M_IMU].due=metric_epoch+sensor_period;
    rx_dropped=uart_overrun=event_coalesced=tx_dropped=rx_peak=tx_peak=0;
}
static void metrics_print(void)
{
    emit("BUILD " DESKLIGHT_BUILD_ID "\r\n");
    char text[256];
    for(unsigned i=0;i<10;i++) {
        task_meter *m=&meter[i];
        snprintf(text,sizeof text,"STAT name=%s n=%lu min_us=%lu avg_us=%lu max_us=%lu jitter_min=%ld jitter_max=%ld skipped=%lu deadline=%lu\r\n",
            meter_names[i],(unsigned long)m->n,(unsigned long)m->min,
            (unsigned long)(m->n ? m->sum/m->n : 0),(unsigned long)m->max,
            (long)m->jitter_min,(long)m->jitter_max,(unsigned long)m->missed,(unsigned long)m->deadline_misses);
        emit(text);
    }
    uint32_t window=measure_us()-metric_epoch;
    snprintf(text,sizeof text,"LOAD t_us=%lu window_us=%lu task_permille=unavailable awake_pct=100 stack_used=%lu stack_reserve=4096 heap_sbrk=%lu rx_drop=%lu ore=%lu tx_drop=%lu rx_peak=%lu tx_peak=%lu coalesced=%lu\r\n",
        (unsigned long)measure_us(),(unsigned long)window,
        (unsigned long)measure_stack_used(),(unsigned long)desk_heap_bytes(),
        (unsigned long)rx_dropped,(unsigned long)uart_overrun,(unsigned long)tx_dropped,
        (unsigned long)rx_peak,(unsigned long)tx_peak,(unsigned long)event_coalesced); emit(text);
}
static bool send_block(const char *text)
{
    return HAL_UART_Transmit(&huart2,(uint8_t *)text,(uint16_t)strlen(text),100)==HAL_OK;
}
static void export_history(void)
{
    uint32_t began=measure_us(); unsigned bytes=0, sent=0;
    GPIOA->BSRR=GPIO_PIN_7;
    while(tx_tail!=tx_head) {
        if(USART2->ISR & USART_ISR_TXE_TXFNF) { USART2->TDR=tx[tx_tail]; tx_tail=(tx_tail+1)&2047u; }
    }
    char text[160];
    for(unsigned n=0;n<history_count;n++) {
        const history_row *h=&history[(history_head+64-history_count+n)%64];
        snprintf(text,sizeof text,"WORK second_ms=%lu active_ms=%lu dim_work_ms=%lu avg_adc=%u angle_tenth=%u unit=relative_not_lux\r\n",
            (unsigned long)h->ms,(unsigned long)h->active_ms,(unsigned long)h->dim_ms,h->avg,h->angle10);
        if(!send_block(text)) break;
        sent++; bytes+=(unsigned)strlen(text);
    }
    GPIOA->BSRR=(uint32_t)GPIO_PIN_7<<16;
    uint32_t elapsed=measure_us()-began; measure_add(&meter[M_EXPORT],elapsed);
    snprintf(text,sizeof text,"EXPORT t_us=%lu rows=%u bytes=%u elapsed_us=%lu end_us=%lu\r\n",
        (unsigned long)began,sent,bytes,(unsigned long)elapsed,(unsigned long)measure_us()); emit(text);
}

static io_result hal_result(HAL_StatusTypeDef result)
{
    return result==HAL_OK ? IO_OK : result==HAL_TIMEOUT ? IO_TIMEOUT : IO_ERROR;
}
/* TXE means the transmit data register can accept a byte. Using it also
 * avoids the simulated C031 TXIS/last-byte NACK behavior seen in captures. */
static io_result wait_bus(I2C_TypeDef *bus,uint32_t wanted,uint32_t began,bool receiving)
{
    for(;;) {
        uint32_t flags=bus->ISR;
        if(flags & (I2C_ISR_BERR|I2C_ISR_ARLO)) return IO_ERROR;
        if(!receiving && (flags & I2C_ISR_NACKF)) return IO_ERROR;
        if(flags & wanted) return IO_OK;
        if(receiving && wanted==I2C_ISR_RXNE && (flags & I2C_ISR_NACKF)) return IO_ERROR;
        if((uint32_t)(HAL_GetTick()-began)>=5) return IO_TIMEOUT;
    }
}
static io_result exchange(I2C_TypeDef *bus,uint8_t address,const uint8_t *out,size_t count,
                          uint8_t *in,size_t incoming)
{
    uint32_t began=HAL_GetTick();
    io_result result=wait_bus(bus,I2C_ISR_TXE,began,false);
    if(result==IO_OK && (bus->ISR & I2C_ISR_BUSY)) result=IO_ERROR;
    if(result!=IO_OK) goto done;
    bus->ICR=I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF;
    bus->CR2=0;
    bus->CR2=(uint32_t)(address<<1)|(uint32_t)(count<<16)|I2C_CR2_START
             |(incoming ? 0u : I2C_CR2_AUTOEND);
    for(size_t i=0;i<count;i++) {
        result=wait_bus(bus,I2C_ISR_TXE,began,false);
        if(result!=IO_OK) goto done;
        bus->TXDR=out[i];
    }
    if(incoming) {
        result=wait_bus(bus,I2C_ISR_TC,began,false);
        if(result!=IO_OK) goto done;
        /* START clears in silicon; explicitly clear it before the next request
         * so the simulator also observes a new repeated-start request. */
        bus->CR2 &= ~I2C_CR2_START;
        bus->CR2=(uint32_t)(address<<1)|(uint32_t)(incoming<<16)
                 |I2C_CR2_RD_WRN|I2C_CR2_AUTOEND|I2C_CR2_START;
        for(size_t i=0;i<incoming;i++) {
            result=wait_bus(bus,I2C_ISR_RXNE,began,true);
            if(result!=IO_OK) goto done;
            in[i]=(uint8_t)bus->RXDR;
        }
    }
    result=wait_bus(bus,I2C_ISR_STOPF,began,incoming!=0);
done:
    if(result!=IO_OK) {
        bus->CR2 |= I2C_CR2_STOP;
        uint32_t stopping=HAL_GetTick();
        while((bus->ISR & I2C_ISR_BUSY) && (uint32_t)(HAL_GetTick()-stopping)<1) {}
        bus->CR1 &= ~I2C_CR1_PE;
        bus->CR1 |= I2C_CR1_PE;
    }
    bus->ICR=I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF;
    bus->CR2=0;
    return result;
}
static io_result bus_read(void *context,uint8_t address,uint8_t reg,uint8_t *bytes,size_t n)
{
    I2C_HandleTypeDef *handle=context;
    return exchange(handle->Instance,address,&reg,1,bytes,n);
}
static io_result bus_write(void *context,uint8_t address,uint8_t reg,uint8_t value)
{
    I2C_HandleTypeDef *handle=context;
    uint8_t payload[]={reg,value};
    return exchange(handle->Instance,address,payload,sizeof payload,NULL,0);
}
static imu_device imu={{&hi2c1,bus_read,bus_write},0x68};

static bool setup_adc(void)
{
    __HAL_RCC_ADC_CLK_ENABLE();
    GPIO_InitTypeDef pin={0};
    pin.Pin=LIGHT_PIN; pin.Mode=GPIO_MODE_ANALOG; pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_PORT,&pin);
    adc.Instance=ADC1;
    adc.Init.ClockPrescaler=ADC_CLOCK_SYNC_PCLK_DIV4;
    adc.Init.Resolution=ADC_RESOLUTION_12B;
    adc.Init.DataAlign=ADC_DATAALIGN_RIGHT;
    adc.Init.ScanConvMode=ADC_SCAN_DISABLE;
    adc.Init.EOCSelection=ADC_EOC_SINGLE_CONV;
    adc.Init.NbrOfConversion=1;
    adc.Init.ExternalTrigConv=ADC_SOFTWARE_START;
    adc.Init.ExternalTrigConvEdge=ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc.Init.Overrun=ADC_OVR_DATA_PRESERVED;
    adc.Init.SamplingTimeCommon1=ADC_SAMPLETIME_160CYCLES_5;
    adc.Init.SamplingTimeCommon2=ADC_SAMPLETIME_160CYCLES_5;
    adc.Init.TriggerFrequencyMode=ADC_TRIGGER_FREQ_LOW;
    ADC_ChannelConfTypeDef channel={0};
    channel.Channel=ADC_CHANNEL_0; channel.Rank=ADC_REGULAR_RANK_1;
    channel.SamplingTime=ADC_SAMPLINGTIME_COMMON_1;
    return HAL_ADC_Init(&adc)==HAL_OK && HAL_ADC_ConfigChannel(&adc,&channel)==HAL_OK
           && HAL_ADCEx_Calibration_Start(&adc)==HAL_OK;
}

static io_result read_light(uint16_t *raw)
{
    HAL_GPIO_WritePin(ADC_MARK_PORT,ADC_MARK_PIN,GPIO_PIN_SET);
    HAL_StatusTypeDef result=HAL_ADC_Start(&adc);
    if(result==HAL_OK) result=HAL_ADC_PollForConversion(&adc,2);
    if(result==HAL_OK) *raw=(uint16_t)HAL_ADC_GetValue(&adc);
    HAL_StatusTypeDef stop=HAL_ADC_Stop(&adc);
    HAL_GPIO_WritePin(ADC_MARK_PORT,ADC_MARK_PIN,GPIO_PIN_RESET);
    return hal_result(result==HAL_OK ? stop : result);
}

static void status(uint32_t now)
{
    char text[256];
    if(desk_status(&state,now,text,sizeof text)) emit(text);
}
static unsigned flags(void)
{
    return (unsigned)state.mode | state.need_light<<2 | state.tilt<<3 | state.fault<<4
        | state.angle_valid<<5 | state.light_valid<<6 | state.dark<<7
        | state.pir_health.fault<<8 | state.light_health.fault<<9 | state.imu_health.fault<<10;
}

static void capture_sample(uint32_t now)
{
    uint8_t id=0,bytes[14]; imu_sample sample;
    io_result result=bus_read(&hi2c1,0x68,0x75,&id,1);
    if(result==IO_OK) result=imu_read(&imu,&sample,bytes);
    char text[256],hex[43];
    if(result==IO_OK) {
        for(unsigned i=0;i<14;++i) snprintf(hex+i*3,4,"%02X ",bytes[i]);
        snprintf(text,sizeof text,
            "SAMPLE t=%lu id=%02X bytes=%sax_mg=%d ay_mg=%d az_mg=%d gx_mdps=%d temp_mC=%d\r\n",
            (unsigned long)now,id,hex,(int)(sample.acceleration[0]*1000),
            (int)(sample.acceleration[1]*1000),(int)(sample.acceleration[2]*1000),
            (int)(sample.rate[0]*1000),(int)(sample.temperature*1000));
    } else snprintf(text,sizeof text,"SAMPLE t=%lu result=%s\r\n",(unsigned long)now,io_name(result));
    emit(text);
}

static void command(desk_command cmd,uint32_t now)
{
    char text[128];
    switch(cmd) {
    case CMD_FLASH_CHECK: {
        unsigned page=journal.latest<0 ? 0u : 1u-(unsigned)journal.latest/64;
        bool ok=flash_erase(NULL,page);
        unsigned blank_bytes=0;
        const volatile uint8_t *p=(const volatile uint8_t *)(0x08007000u+page*2048u);
        for(unsigned i=0;i<2048;i++) blank_bytes+=p[i]==255;
        snprintf(text,sizeof text,"ERASE page=%u result=%s ff_bytes=%u sr=%08lX\r\n",
            page+14,ok ? "OK" : "ERROR",blank_bytes,(unsigned long)FLASH->SR);
        emit(text); return;
    }
    case CMD_STATS: metrics_print(); return;
    case CMD_METRICS_RESET: metrics_reset(); emit("METRICS reset\r\n"); return;
    case CMD_HISTORY: export_history(); return;
    case CMD_SAVE: checkpoint(); return;
    case CMD_JOURNAL:
        snprintf(text,sizeof text,"JOURNAL seq=%lu valid=%lu damaged=%lu active_ms=%lu dim_ms=%lu\r\n",
            (unsigned long)journal.sequence,(unsigned long)journal.accepted,(unsigned long)journal.damaged,
            (unsigned long)journal.totals.active_ms,(unsigned long)journal.totals.dim_work_ms);
        emit(text); return;
    case CMD_FAST: sensor_period=20000; metrics_reset(); emit("RATE sensor_us=20000\r\n"); return;
    case CMD_NORMAL: sensor_period=100000; metrics_reset(); emit("RATE sensor_us=100000\r\n"); return;
    case CMD_SAMPLE: capture_sample(now); return;
    case CMD_NACK: {
        uint8_t byte=0;
        io_result result=bus_read(&hi2c1,0x69,0x75,&byte,1);
        snprintf(text,sizeof text,"PROBE t=%lu address=69 result=%s\r\n",(unsigned long)now,io_name(result));
        emit(text); return;
    }
    case CMD_IMU_OFF: forced_imu=true; break;
    case CMD_IMU_ON: forced_imu=false; break;
    case CMD_ADC_OFF: forced_adc=true; break;
    case CMD_ADC_ON: forced_adc=false; break;
    case CMD_IMU_PAUSE: paused_imu=true; break;
    case CMD_IMU_RESUME: paused_imu=false; break;
    case CMD_ADC_PAUSE: paused_adc=true; break;
    case CMD_ADC_RESUME: paused_adc=false; break;
    default:
        snprintf(text,sizeof text,"REPLY t=%lu %s\r\n",(unsigned long)now,console_apply(&state,cmd,now));
        emit(text); status(now); return;
    }
    snprintf(text,sizeof text,"DIAGNOSTIC t=%lu forced_imu=%u forced_adc=%u paused_imu=%u paused_adc=%u\r\n",
        (unsigned long)now,forced_imu,forced_adc,paused_imu,paused_adc);
    emit(text);
}

void board_begin(void)
{
    measure_begin();
    GPIO_InitTypeDef pin={0};
    pin.Pin=PIR_PIN; pin.Mode=GPIO_MODE_IT_RISING; pin.Pull=GPIO_PULLDOWN;
    HAL_GPIO_Init(PIR_PORT,&pin);
    HAL_NVIC_SetPriority(EXTI0_1_IRQn,1,0); HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
    pin.Pin=ADC_MARK_PIN|GPIO_PIN_1|GPIO_PIN_6|GPIO_PIN_7; pin.Mode=GPIO_MODE_OUTPUT_PP; pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(ADC_MARK_PORT,&pin);
    HAL_GPIO_WritePin(Led_GPIO_Port,Led_Pin,GPIO_PIN_RESET);
    adc_ready=setup_adc();
    uint32_t now=HAL_GetTick();
    desk_init(&state,now);
    status_due=init_due=history_due=save_due=accounting_tick=now;
    work_store store={NULL,flash_read,flash_program,flash_erase};
    worklog_open(&journal,store); totals=journal.totals;
    metrics_reset();
    SET_BIT(USART2->CR1,USART_CR1_RXNEIE_RXFNEIE);
    HAL_NVIC_SetPriority(USART2_IRQn,1,0); HAL_NVIC_EnableIRQ(USART2_IRQn);
    emit("DeskLight ready " DESKLIGHT_BUILD_ID " timer=SysTick_1ms cpu=48MHz\r\n"); status(now);
    previous_flags=flags();
}

void board_step(void)
{
    uint32_t loop_began=measure_us(), began;
    if(tx_tail!=tx_head && (USART2->ISR & USART_ISR_TXE_TXFNF)) {
        USART2->TDR=tx[tx_tail]; tx_tail=(tx_tail+1)&2047u;
    }
    if(rx_overflow) {
        uint32_t saved=__get_PRIMASK(); __disable_irq();
        rx_tail=rx_head; rx_overflow=false;
        __set_PRIMASK(saved);
        console.length=0; console.overflow=true;
    }
    unsigned used=(rx_head-rx_tail)&127u;
    if(used>rx_peak) rx_peak=used;
    for(unsigned budget=0;budget<32 && rx_tail!=rx_head;budget++) {
        uint32_t received=rx_time[rx_tail];
        char byte=(char)rx[rx_tail]; rx_tail=(rx_tail+1)&127u;
        desk_command cmd=console_feed(&console,byte);
        if(cmd!=CMD_NONE) {
            began=measure_us();
            uint32_t delay=began-received;
            command(cmd,HAL_GetTick());
            if(cmd!=CMD_METRICS_RESET && cmd!=CMD_FAST && cmd!=CMD_NORMAL) {
                uint32_t duration=measure_us()-began;
                measure_add(&meter[M_COMMAND],duration);
                if(cmd!=CMD_HISTORY && cmd!=CMD_SAVE && delay+duration>200000) meter[M_COMMAND].deadline_misses++;
                char text[128];
                snprintf(text,sizeof text,"COMMAND kind=%u received_us=%lu begin_us=%lu enqueue_done_us=%lu\r\n",
                    (unsigned)cmd,(unsigned long)received,(unsigned long)began,(unsigned long)measure_us()); emit(text);
            }
            break;
        }
    }
    uint32_t now=HAL_GetTick();
    if(!started && (uint32_t)(now-init_due)>=100) {
        began=measure_us();
        uint8_t id=0;
        io_result result=imu_start(&imu,&id);
        imu_ready=result==IO_OK; started=true; init_due=HAL_GetTick();
        char text[128];
        snprintf(text,sizeof text,"SELFTEST device=MPU6050 result=%s address=68 id=%02X\r\n",io_name(result),id);
        emit(text);
        emit(adc_ready ? "SELFTEST device=LDR result=ADC_READY\r\n" : "SELFTEST device=LDR result=ERROR\r\n");
        emit("SELFTEST device=PIR result=GPIO_READABLE\r\n");
        measure_add(&meter[M_INIT],measure_us()-began);
    }
    if((int32_t)(measure_us()-meter[M_PIR].due)>=0) {
        began=measure_us(); measure_release(&meter[M_PIR],began,20000);
        desk_pir(&state,HAL_GPIO_ReadPin(PIR_PORT,PIR_PIN)==GPIO_PIN_SET,HAL_GetTick());
        if(event_pending) {
            uint32_t saved=__get_PRIMASK(); __disable_irq();
            uint32_t edge=event_us; event_pending=false; __set_PRIMASK(saved);
            uint32_t reacted=measure_us(), delay=reacted-edge;
            measure_add(&meter[M_REACTION],delay);
            if(delay>100000) meter[M_REACTION].deadline_misses++;
            GPIOA->ODR^=GPIO_PIN_1;
            char text[128];
            snprintf(text,sizeof text,"EVENT edge_us=%lu react_us=%lu delay_us=%lu\r\n",
                (unsigned long)edge,(unsigned long)reacted,(unsigned long)delay); emit(text);
        }
        measure_add(&meter[M_PIR],measure_us()-began);
    }
    if(started && (int32_t)(measure_us()-meter[M_ADC].due)>=0) {
        began=measure_us(); measure_release(&meter[M_ADC],began,sensor_period);
        if(!paused_adc) {
            uint16_t raw=0;
            io_result result=adc_ready && !forced_adc ? read_light(&raw) : IO_TIMEOUT;
            desk_light(&state,result==IO_OK,raw,HAL_GetTick());
        }
        measure_add(&meter[M_ADC],measure_us()-began);
        if((int32_t)(measure_us()-meter[M_ADC].due)>0) meter[M_ADC].deadline_misses++;
    }
    if(started && (int32_t)(measure_us()-meter[M_IMU].due)>=0) {
        began=measure_us(); measure_release(&meter[M_IMU],began,sensor_period);
        GPIOA->BSRR=GPIO_PIN_6;
        if(!paused_imu) {
            if(!imu_ready && !forced_imu && (uint32_t)(now-init_due)>=1000) {
                uint8_t id=0; imu_ready=imu_start(&imu,&id)==IO_OK; init_due=HAL_GetTick();
            }
            imu_sample sample; uint8_t bytes[14];
            io_result result=imu_ready && !forced_imu ? imu_read(&imu,&sample,bytes) : IO_ERROR;
            desk_imu(&state,result==IO_OK,result==IO_OK ? sample.acceleration : NULL,HAL_GetTick());
            if(result!=IO_OK && !forced_imu) imu_ready=false;
        }
        GPIOA->BSRR=(uint32_t)GPIO_PIN_6<<16;
        measure_add(&meter[M_IMU],measure_us()-began);
        if((int32_t)(measure_us()-meter[M_IMU].due)>0) meter[M_IMU].deadline_misses++;
    }
    began=measure_us();
    account(HAL_GetTick());
    now=HAL_GetTick(); desk_update(&state,now);
    HAL_GPIO_WritePin(Led_GPIO_Port,Led_Pin,led_update(&led,&state,now) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    unsigned current=flags();
    if(current!=previous_flags || (uint32_t)(now-status_due)>=1000) {
        status(now); status_due=now; previous_flags=current;
    }
    if((uint32_t)(now-history_due)>=1000) {
        history_due=now;
        history[history_head]=(history_row){now,totals.active_ms,totals.dim_work_ms,
            (uint16_t)(state.sum/5),state.angle_valid ? (uint16_t)(state.angle*10) : 65535};
        history_head=(history_head+1)%64;
        if(history_count<64) history_count++;
    }
    measure_add(&meter[M_CONTROL],measure_us()-began);
    if((uint32_t)(now-save_due)>=60000) {
        save_due=now; began=measure_us(); checkpoint(); periodic_flash_sum+=measure_us()-began;
    }
    measure_add(&meter[M_LOOP],measure_us()-loop_began);
}
