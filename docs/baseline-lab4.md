## Новий S3-trigger-v2-01, 31.10.2026

Окремий запуск того самого ELF d4c6ed31: STATUS 6508–167043 мс, 120 повних UART/D1-узгоджених експортів, 119 різних фронтів D3 і 119 однозначних відповідей D7. D3–D7: min 711,335 мс, avg 762,703 мс, max 826,843 мс. Це маркер обробки у Wokwi, не фізичний LED. Цільова межа 100 мс не досягнута. RX drop/ORE/TX drop 0 для конкретного потоку; PIR skipped 4464, ADC 790, IMU 791. SAVE двічі ERROR, фізичне збереження і power cut не підтверджені. Попередні серії не об'єднано. Дані: `docs/metrics/lab3/S3-trigger-v2-01/`, `docs/captures/lab3/S3-trigger-v2-01.vcd`, commit 5599909.

---

## Стан доказів 31.10.2026

Базова прошивка ELF SHA-256 `d4c6ed31844d8988d0530fddeb7923e3aed11502e20d534d9e1c462bf6516843`, STM32C031C6 і super loop збережені. S2 охоплює 3600,188 с за STATUS і має окремий VCD. Раніше S3 дав лише 23 узгоджені події; новий S3-trigger-01 має 116 узгоджених EXPORT і 0 подій D3 у них. Вимога 30 подій не підтверджена. SAVE ERROR, фізичні power-cut і вимірювання не підтверджені. Не переносити ці результати на RTOS; її реалізація належить наступній лабораторній.

---

# Базова лінія для ЛР4, DeskLight

Кандидат `c26a59c` після негативного regression використовує SysTick із роздільністю 1 мс. Згадки TIM3 нижче належать попередньому кандидату та не є валідною базовою лінією. Підтвердження нового часу та фінальна таблиця очікують власного запуску. Завантаження задач у відсотках цим методом недоступне; короткі тривалості слід отримувати з VCD.

ЛР4 тут не реалізується. Остаточна baseline потребує завершення реальних S1/S2/S3. Поки runtime-поля не заповнено, вони мають статус «не виміряно».

## Незмінні умови

STM32C031C6, HSI/SYSCLK/PCLK 48 МГц; UART 115200 8N1; I²C 100 кГц; LDR AO PA0, PIR PB0, MPU PB8/PB9. TIM3 1 МГц, CNT fold із SysTick service. Фінальний source digest з банера, ELF/HEX/BIN SHA256, diagram SHA256, compiler version мають бути в manifest. Не порівнювати збірки -Og ЛР2 та -Os ЛР3 як чистий ефект ОС.

Analyzer D0=PA6 IMU, D1=PA7 export/flash, D2=PA2 TX, D3=PB0 PIR, D4=PA4 ADC, D5=PA5 LED, D6=PA3 RX, D7=PA1 reaction toggle. VCD timestamps є simulator time; 1 ns timescale не означає 1 GHz sampling. Для SR периферії застосувати resampling >=10 MHz; для довгої PIR/LED ділянки досить 100 kHz. Не брати host arrival time за час MCU.

## Запуски

1. S1. Окремий reset, 500 lux, +Z, без PIR. Після ready виконати metrics reset, збирати >=210 с MCU, stats. Період ADC/IMU 100 мс дає номінально 2100 releases; підтвердити фактичні >=2000 для кожного та min/avg/max. PIR за періоду 20 мс дає номінально 10500. Зберегти UART і VCD.
2. S2. Окремий reset, 0.1 lux, періодичний PIR не частіше ніж раз на 7 с, зрідка mode adjust / zero / mode auto / journal. Збирати >=3600 с MCU, stats кожні 60 с, перевірити heap/stack і sequence/readback. Не підміняти годину прискореним host-тестом.
3. S3. Та сама збірка. Спочатку 65 с, щоб RAM-історія мала 64 реальні зрізи. metrics reset. history під час періодичного PIR та потоку mode-команд. >=30 PIR rising в різних фазах export, >=300 с загального runtime. Повторити з rate fast; не порівнювати fast з normal без позначки.
4. Відмова. fault imu on, 3 с, fault imu off; pause imu on, 1 с, pause imu off; окремо probe nack. Це software injection / відсутня адреса, не утримання SDA. Для реального stuck-SDA потрібний окремий стендовий дослід.
5. Межа UART. Після наповнення history повторити частоти команд 1,2,5,10,20,50/с окремими вікнами >=30 с з metrics reset. Першим порушенням вважати dropped RX, deadline FR-09 або пропущений PIR/ADC/IMU. Записати фактично надіслані байти й MCU stamps, не лише заданий host rate.
6. Старт. За VCD від reset до першого успіху IMU, п'ятого ADC та першого ready. Пізню готовність IMU перевірити контрольованим відключенням з чітким позначенням методу.
7. Power interruption. Save нормальний, reset, journal. Потім перервати реальне програмування/erase, зберегти сирі flash bytes до/після reset, перевірити CRC і попередню committed sequence. Restart Wokwi може завантажити ELF заново й очистити flash, тому його не називати втратою живлення з persistence. Host cuts є окремим алгоритмічним доказом.

## Метрики й гіпотези

| Метрика | S1 | S2 | S3 |
|---|---|---|---|
| task/loop min/avg/max | не виміряно | не виміряно | не виміряно |
| PIR/command latency max/p99 | не виміряно | не виміряно | не виміряно |
| jitter/skipped/deadline | не виміряно | не виміряно | не виміряно |
| CPU task share / awake | не виміряно / 100% за кодом | те саме | те саме |
| runtime stack/heap peak | не виміряно | не виміряно | не виміряно |

Статична пам'ять і stack .su зберігаються в docs/metrics/lab3. Статична оцінка не замінює runtime watermark. Реєстр гіпотез містить docs/limitations.md; очікувані числові цілі є гіпотезами, не результатами ЛР4. Зберегти ті самі GPIO, часові джерела, сценарії й спосіб аналізу для чесного повторення.
