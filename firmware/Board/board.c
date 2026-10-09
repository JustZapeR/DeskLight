#include "board.h"
#include "../Desk/console.h"
#include "../Sensors/imu.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
static ADC_HandleTypeDef adc;
static desk state;
static desk_console console;
static desk_led led;
static uint8_t tx[2048], rx[128];
static unsigned tx_head, tx_tail;
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_overflow;
static uint32_t pir_due, sensors_due, status_due, init_due;
static bool adc_ready, imu_ready, started;
static bool forced_imu, forced_adc, paused_imu, paused_adc;
static unsigned previous_flags;

static void emit(const char *text)
{
    size_t n=strlen(text);
    if (n >= sizeof tx - ((tx_head-tx_tail)&2047u)) return;
    while(*text) { tx[tx_head]=*text++; tx_head=(tx_head+1)&2047u; }
}

void USART2_IRQHandler(void)
{
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t byte=(uint8_t)USART2->RDR;
        unsigned next=(rx_head+1)&127u;
        if(next!=rx_tail) { rx[rx_head]=byte; rx_head=next; }
        else rx_overflow=true;
    }
    USART2->ICR=USART_ICR_ORECF;
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
    GPIO_InitTypeDef pin={0};
    pin.Pin=PIR_PIN; pin.Mode=GPIO_MODE_INPUT; pin.Pull=GPIO_PULLDOWN;
    HAL_GPIO_Init(PIR_PORT,&pin);
    pin.Pin=ADC_MARK_PIN; pin.Mode=GPIO_MODE_OUTPUT_PP; pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(ADC_MARK_PORT,&pin);
    HAL_GPIO_WritePin(Led_GPIO_Port,Led_Pin,GPIO_PIN_RESET);
    adc_ready=setup_adc();
    uint32_t now=HAL_GetTick();
    desk_init(&state,now);
    pir_due=sensors_due=status_due=init_due=now;
    SET_BIT(USART2->CR1,USART_CR1_RXNEIE_RXFNEIE);
    HAL_NVIC_SetPriority(USART2_IRQn,1,0); HAL_NVIC_EnableIRQ(USART2_IRQn);
    emit("DeskLight ready\r\n"); status(now);
    previous_flags=flags();
}

void board_step(void)
{
    if(tx_tail!=tx_head && (USART2->ISR & USART_ISR_TXE_TXFNF)) {
        USART2->TDR=tx[tx_tail]; tx_tail=(tx_tail+1)&2047u;
    }
    if(rx_overflow) { console.overflow=true; rx_overflow=false; }
    while(rx_tail!=rx_head) {
        char byte=(char)rx[rx_tail]; rx_tail=(rx_tail+1)&127u;
        desk_command cmd=console_feed(&console,byte);
        if(cmd!=CMD_NONE) command(cmd,HAL_GetTick());
    }
    uint32_t now=HAL_GetTick();
    if(!started && (uint32_t)(now-init_due)>=100) {
        uint8_t id=0;
        io_result result=imu_start(&imu,&id);
        imu_ready=result==IO_OK; started=true; init_due=HAL_GetTick();
        char text[128];
        snprintf(text,sizeof text,"SELFTEST device=MPU6050 result=%s address=68 id=%02X\r\n",io_name(result),id);
        emit(text);
        emit(adc_ready ? "SELFTEST device=LDR result=ADC_READY\r\n" : "SELFTEST device=LDR result=ERROR\r\n");
        emit("SELFTEST device=PIR result=GPIO_READABLE\r\n");
    }
    if((uint32_t)(now-pir_due)>=20) {
        pir_due=now; desk_pir(&state,HAL_GPIO_ReadPin(PIR_PORT,PIR_PIN)==GPIO_PIN_SET,now);
    }
    if(started && (uint32_t)(now-sensors_due)>=100) {
        sensors_due=now;
        if(!paused_adc) {
            uint16_t raw=0;
            io_result result=adc_ready && !forced_adc ? read_light(&raw) : IO_TIMEOUT;
            desk_light(&state,result==IO_OK,raw,HAL_GetTick());
        }
        if(!paused_imu) {
            if(!imu_ready && !forced_imu && (uint32_t)(now-init_due)>=1000) {
                uint8_t id=0; imu_ready=imu_start(&imu,&id)==IO_OK; init_due=HAL_GetTick();
            }
            imu_sample sample; uint8_t bytes[14];
            io_result result=imu_ready && !forced_imu ? imu_read(&imu,&sample,bytes) : IO_ERROR;
            desk_imu(&state,result==IO_OK,result==IO_OK ? sample.acceleration : NULL,HAL_GetTick());
            if(result!=IO_OK && !forced_imu) imu_ready=false;
        }
    }
    now=HAL_GetTick(); desk_update(&state,now);
    HAL_GPIO_WritePin(Led_GPIO_Port,Led_Pin,led_update(&led,&state,now) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    unsigned current=flags();
    if(current!=previous_flags || (uint32_t)(now-status_due)>=1000) {
        status(now); status_due=now; previous_flags=current;
    }
}
