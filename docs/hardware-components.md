# Компоненти та конфігурація DeskLight

Єрмак Олександр Віталійович, ІО-32.

## Збережений опис HAL-шаблону ЛР1

Цей розділ фіксує стан ЛР1 до інтеграції. Поточні параметри ЛР2 наведено в [pinout](pinout.md), [архітектурі](architecture.md) та [аналізі записів](captures/analysis.md). Таблиця нижче не описує поточну прошивку. Конфігурацію перевірено читанням firmware/firmware.ioc, Core/Src/main.c, stm32c0xx_hal_msp.c, Core/Inc/main.h та stm32c0xx_hal_conf.h, firmware/Makefile і лінкерного скрипта.

| Вузол | Наявне налаштування | Підстава |
|---|---|---|
| MCU | STM32C031C6, Cortex-M0+, LQFP48, 32 KiB Flash, 12 KiB RAM; компілятор -mcpu=cortex-m0plus -mthumb, без FPU | .ioc, Makefile, linker; [ST DS13867 Rev 4, с. 1, 30](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=1) |
| Clock | HSI 48 МГц, HSI_DIV1, SYSCLK/AHB/APB1 DIV1, FLASH_LATENCY_1; SysTick HAL | main.c SystemClock_Config; [ST DS13867, с. 1](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=1) |
| HAL | STM32Cube FW_C0 V1.0.1 в .ioc; HAL 1.0.1 у Drivers/.../Src/stm32c0xx_hal.c; CubeMX 6.8.1 | Локальні .ioc та HAL version macros |
| USART2 | PA2 TX, PA3 RX, AF1; 115200, 8 біт, без парності, 1 stop bit, TX_RX, без flow control, oversampling 16 | main.c, MSP; [ST DS13867, с. 35, табл. 13](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=35) |
| printf | syscalls.c _write викликає __io_putchar, який надсилає кожен байт через HAL_UART_Transmit з HAL_MAX_DELAY | Локальні main.c і syscalls.c; командного UART інтерфейсу поки немає |
| LED | PA5, output push-pull, GPIO_NOPULL, VERY_HIGH speed, початковий HIGH. Цикл перемикає стан щосекунди; повний період близько 2 с | main.c, main.h; [Wokwi Nucleo, розділ Onboard LED](https://docs.wokwi.com/parts/board-st-nucleo-c031c6) підтверджує LED LD4 PA5, HIGH вмикає |
| I²C1 | PB8 SCL/PB9 SDA, AF6, open-drain, без внутрішніх pull-up; 7-бітна адресація, Timing 0x20303E5D, analog filter ON, digital filter 0 | main.c, MSP, .ioc; [ST DS13867, с. 36, табл. 15](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=36) |
| Кнопка | PC13, rising EXTI, GPIO_NOPULL, IRQ EXTI4_15 увімкнено; продуктової обробки немає | main.c, main.h, stm32c0xx_it.c |
| ADC | HAL_ADC_MODULE_ENABLED закоментований, MX_ADC_Init і ADC джерел у Makefile немає | stm32c0xx_hal_conf.h і Makefile; канали світла ще не існують |
| Схема | Один board-st-nucleo-c031c6. Serial Monitor TX → PA3; RX ← PA2; датчики відсутні | diagram.json |
| Артефакти | firmware/build/firmware.elf, .hex, .bin; wokwi.toml завантажує HEX і вказує ELF | Локальний Makefile і wokwi.toml; [Wokwi project config](https://docs.wokwi.com/vscode/project-config) підтримує HEX/BIN/ELF STM32 |

Точна частота SCL не виміряна. Timing register не є самостійним доказом 100 кГц без урахування такту, фільтрів і фронтів. На ЛР2 ціль 100 кГц треба звірити в CubeMX та логічним аналізатором. HAL перевіряє повернення init і викликає Error_Handler при помилці, але успішна компіляція не підтверджує HAL_OK під час виконання.

У [Wokwi Nucleo, Simulation features](https://docs.wokwi.com/parts/board-st-nucleo-c031c6) є GPIO, USART, I²C master, ADC та SysTick. RCC реалізований частково, DMA не реалізований. На ЛР2 планується опитування HAL без DMA. Це перелік можливостей симулятора, не результат запуску DeskLight.

## Кандидати, розглянуті на ЛР1

Схема нижче є планом. Піни PA0 та PB0 треба додати через CubeMX; прошивку, .ioc і diagram.json на ЛР1 не змінюємо. Наявні PA2/PA3 та PA5 залишаємо для UART та LED, PB8/PB9 для I²C.

| Модель | Підключення в плані | Для чого та обмеження |
|---|---|---|
| wokwi-pir-motion-sensor | OUT → PB0 GPIO input, VCC/GND → шини моделі | FR-02/03, інтервал активності за HIGH. Фізичний аналог типу HC-SR501 не обраний за виробником; параметри його живлення й струму не можна переносити з моделі |
| wokwi-photoresistor-sensor | AO → PA0 ADC_IN0, VCC → 3,3 В, GND → GND; DO не використовується | FR-04..06, відносне світло. [ST DS13867, с. 32](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=32) підтверджує ADC_IN0 PA0. План 12 bit, 0..4095, VREF+ 3,3 В; AO повинен бути в 0..VREF+ |
| wokwi-mpu6050 | SCL PB8, SDA PB9, VCC 3,3 В, GND GND, AD0 GND; INT/XDA/XCL не потрібні | FR-07..09, нахил основи. 0x68, опитування без IRQ. Для фізичного монтажу pull-up SDA/SCL до 3,3 В та сумісність модуля перевіряються окремо |

PIR не рахує людей. LDR слід розташувати біля робочої поверхні, MPU6050 закріпити на основі лампи. Нахил основи не дорівнює положенню рухомого плафона. У симуляції прискорення IMU і світло задаються незалежно, тому тест не доводить фізичну залежність освітлення від повороту лампи.

### PIR та його альтернатива

[Офіційна модель Wokwi PIR](https://docs.wokwi.com/parts/wokwi-pir-motion-sensor), розділи Pin names, Attributes, Using the sensor: OUT цифровий; delayTime за замовчуванням 5 с, inhibitTime 1,2 с, retrigger подовжує HIGH. У тесті натискають Simulate Motion. Ці інтервали належать датчику й не замінюють 30-секундний таймер DeskLight після останнього HIGH. Register map для GPIO-сигналу відсутня.

Для реального пристрою розглянуто [Panasonic PaPIRs EKMC1601111, Specifications](https://na.industrial.panasonic.com/products/sensors/motion-displacement-sensors/series/149463/model/149603). Цифровий вихід, заявлений standby 170 μA та відстань 5 м. Це інший датчик, не HC-SR501 і не точний еквівалент моделі. Перед фізичним монтажем потрібні його конкретний datasheet, напруга, pinout і умови виходу. У ЛР1 немає реальної плати, тому загальний струм PIR не заявлено. Кнопка як альтернатива проста для GPIO-тесту, але потребує ручного підтвердження активності.

### LDR та його альтернатива

[Офіційна модель Wokwi LDR](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor), Pin names/Attributes/Operation: AO від дільника LDR+10 кОм; default rl10=50 кОм, gamma=0,7. При меншому світлі AO зростає. Таблиця документації розрахована для 5 В та Arduino-кодів, її числа не копіюємо в 3,3 В / 12 bit HAL. За планом використовуємо raw ADC та власні початкові пороги 2400/2000. Модельних lux достатньо для постановки тесту, але вони не є калібруванням реального LDR.

Фізичний кандидат [Senba GL5528, офіційна таблиця Specification](https://ru.senbasensor.com/visible-light-sensor/56.html): опір при 10 lux 10..20 кОм, dark 1 МОм, response increase/decrease 20/30 мс. Це не default 50 кОм Wokwi. Офіційна вебтаблиця не має нумерації сторінок; окремий PDF конкретної партії тут не підтверджено. GL5528 є пасивним двовивідним резистором, не модулем AO/DO; register map та сталий струм живлення відсутні. Для дільника 3,3 В з 10 кОм теоретична верхня межа струму 3,3/10000=0,33 мА, без струму компаратора модуля. Потенціометр відкинуто як основний датчик, але він придатний для відтворюваних кодів ADC на межах.

### MPU6050 та його альтернатива

[Wokwi MPU6050](https://docs.wokwi.com/parts/wokwi-mpu6050), Pin names/Attributes/Units: AD0 LOW дає 0x68, HIGH 0x69; accelX/Y/Z у g, rotationX/Y/Z у °/с. Для статичного нахилу змінюємо вектор прискорення, а не задаємо число градусів у rotationX. У стані +Z початкові значення 0,0,1 g.

[TDK/InvenSense PS-MPU-6000A-00 Rev 3.4, с. 14](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf#page=14) задає VDD 2,375..3,46 В, типовий струм gyro+accel без DMP 3,8 мА, accel-only 500 μA. Це параметри чіпа; струм regulator/LED breakout до них додається. Не подавати 5 В безпосередньо на чіп. За [с. 33, I²C interface](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf#page=33) MPU6050 має 7-бітну адресу і I²C до 400 кГц. Проєктна ціль 100 кГц нижча за цю межу.

| Регістр ЛР2 | Призначення | Джерело |
|---|---|---|
| WHO_AM_I 0x75 | Очікувати 0x68 незалежно від AD0; відрізняти ідентифікатор від адреси шини | [RM-MPU-6000A-00 Rev 4.2, с. 45, §4.32](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf#page=45) |
| ACCEL_CONFIG 0x1C | AFS_SEL=0, ±2 g | [Register map, с. 15, §4.5](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf#page=15) |

Значення VDD та струму, WHO_AM_I і ACCEL_CONFIG звірено з індексованим текстом офіційних PDF. Повну карту регістрів поки не перевірено. Перед реалізацією фізичного датчика потрібно отримати доступну офіційну редакцію datasheet та register map, звірити послідовність виходу зі sleep, адреси вимірювань і масштаб перетворення. У ЛР1 жодної транзакції з датчиком немає.

У HAL I²C API адресу 0x68 слід передавати як 0x68<<1 відповідно до коментарів DevAddress у наявному Drivers/STM32C0xx_HAL_Driver/Src/stm32c0xx_hal_i2c.c. Для читання регістрів потрібна 8-бітна адреса пам'яті. Це план, не виконана транзакція. Невеликий tilt-switch відкинуто: він дає дискретний стан, тоді як PRD потребує кута та калібрування.

## Що ще виміряти

Параметри струму всього пристрою та можливість живлення від 3V3 Nucleo не перевірені на обладнанні. Wokwi не замінює електричну перевірку breakout, pull-up, VREF+ та рівнів сигналів. У ЛР2 окремо підтвердити SCL, WHO_AM_I, полярність ADC, еталон IMU й час реакції, потім перейти до одночасного опитування за [PRD](PRD.md).

## Реалізовано на ЛР2

GPIO PIR PB0, ADC_IN0 PA0 та MPU PB8/PB9 працюють одночасно. Runtime printf замінено чергою UART. Самоперевірка MPU включає WHO_AM_I і конфігурацію. Обрано PWR_MGMT_1 0x6B=0, CONFIG 0x1A=3, GYRO_CONFIG 0x1B=0, ACCEL_CONFIG 0x1C=0, SMPLRT_DIV 0x19=9. DLPF 3 дає accel 44 Гц і gyro 42 Гц; дільник задає 100 Гц. Burst від 0x3B має 14 байтів accel/temp/gyro. Масштаби ±2 g: 16384 LSB/g, ±250°/с: 131 LSB/(°/с), температура raw/340+36.53°C.

Повну карту для цієї реалізації перевірено за оригінальним InvenSense [RM-MPU-6000A-00 rev 4.0](https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf), с. 13–15, 30–32, 41, 46. PDF є копією документа виробника на SparkFun. Попередній опис rev 4.2 вище є історичним джерелом ЛР1. До/після HAL обміну, signed bytes та незалежне декодування наведено в аналізі.
