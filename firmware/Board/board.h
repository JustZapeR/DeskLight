#ifndef DESKLIGHT_BOARD_H
#define DESKLIGHT_BOARD_H
#include "main.h"
#define PIR_PORT GPIOB
#define PIR_PIN GPIO_PIN_0
#define LIGHT_PORT GPIOA
#define LIGHT_PIN GPIO_PIN_0
#define ADC_MARK_PORT GPIOA
#define ADC_MARK_PIN GPIO_PIN_4
void board_begin(void);
void board_step(void);
#endif
