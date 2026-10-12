# Аналіз реальних записів DeskLight

Записи виконано 13.10.2026 у Wokwi на єдиній робочій копії. Форма B, фізичного стенда немає. Фінальні light-activity, tilt-zero-recovery та adjust-repeat-activity відповідають тому самому runtime коду, що в 633422f після зміни Git dates. Ранні diagnostic captures належать проміжним збіркам і не мають хешу фінального ELF.

## Канали, захоплення і походження

D0=SCL PB8, D1=SDA PB9, D2=UART TX PA2, D3=PIR PB0, D4=ADC_MARK PA4, D5=LED PA5, D6=UART RX PA3. Спільний GND, analyzer buffer 4M. Запис починається зі стартом симулятора, окремого hardware trigger немає. UI дії не мають digital channel timestamp. Час усіх вимірів є часом симулятора, не wall clock.

Повні VCD та decoded JSON/UART/I²C знаходяться у Lab02/Work/captures відносно каталогу студента SP. [manifest.json](manifest.json) містить їх SHA256, тривалості та всі offsets коротких уривків. Первинні записи не редагувалися. Уривки містять стан на початку і ті самі переходи; GPIO excerpts відкидають тільки сторонні канали. Усі переходи звірено з джерелом, PASS у [sigrok-fixture.log](../validation/sigrok-fixture.log).

VCD timescale 1 ns означає точність представлення timestamps. SR є регулярним resampling: I²C/UART 10 MHz, 100 samples на SCL period і приблизно 87 samples на UART bit; ADC 1 MHz, LED 100 kHz, PIR 10 kHz. 202 с у 10 MHz з байтом на sample потребували б близько 2 GB, тому fast bus збережено короткими уривками. GPIO SR не використовуються для fast bus decode. Буфер Wokwi не названо рівномірним 1 GHz sampling.

| Фінальний VCD | Тривалість | ADC median period | ADC_MARK median width |
|---|---|---|---|
| light-activity | 202.813134725 с | 99.982425 мс | 71.425 мкс |
| tilt-zero-recovery | 221.354477425 с | 99.999175 мс | 71.425 мкс |
| adjust-repeat-activity | 199.337563875 с | 99.993925 мс | 71.425 мкс |

Повні raw bytes і переходи є джерелом чисел. PulseView sessions відкривали реальні SR, а не намальовані осцилограми. Зображення docs/lab_2/img є тільки crop фактичних screenshots; [provenance](../lab_2/screenshot-provenance.json) фіксує original hash та crop box.

## Самоперевірка й meaningful I²C

На старті UART отримано DeskLight ready, STATUS t=1 idle invalid, SELFTEST MPU6050 OK address=68 id=68, LDR ADC_READY, PIR GPIO_READABLE. t=205 два успіхи дали IMU valid, t=505 п'ять ADC samples дали light_valid. Це доступність моделі, не діагностика справності фізичних PIR/LDR.

WHO_AM_I: START, D0 ACK, 75 ACK, repeated START, D1 ACK, 68 NACK, STOP. D0/D1 є 8-бітними wire bytes, обидва означають 7-бітну адресу 0x68, R/W 0/1. Конфігурація 6B=00, 1A=03, 1B=00, 1C=00, 19=09 підтверджена startup-i2c.vcd та i2c.sigrok.txt. Це wake, DLPF, ±250°/с, ±2 g і sample divider 100 Hz.

За оригінальною InvenSense [RM rev 4.0](https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf), с. 30–32, послідовність від 0x3B має AX, AY, AZ, temperature, GX, GY, GZ, по два bytes MSB first.

Реальний burst у tilt-zero-recovery: repeated-start read 139299982650..139301347650 ns. Master читав:

```text
F9 9A 00 00 3C CC EF 5C 00 00 00 00 00 00
```

ACK після перших 13 bytes, master NACK після останнього, STOP. Повна пара write-register/read від 139299777875 до 139301347650 ns триває 1.569775 мс; read leg 1.365 мс. Бус потрібен не тільки для ID: драйвер розбирає дані у g, °/с та °C.

### Ручне декодування без annotations

У i2c-manual.vcd SCL D0 і SDA D1 показані без decoder. START є падінням SDA при HIGH SCL. На восьми rising SCL першого адресного byte SDA має 11010000, тобто 0xD0. Відкинути R/W=0, отримати 0x68. Дев'ятий bit LOW є ACK. Наступний register byte 00111011 дорівнює 0x3B. Repeated START не має STOP між address phases. Далі 11010001 дорівнює 0xD1, read адреси 0x68. Перші data bits 11111001 і 10011010 дають F9 та 9A.

F99A unsigned =63898; signed int16 =63898−65536=−1638. ax=−1638/16384=−0.0999755859375 g. 3CCC=15564, az=0.949951171875 g. EF5C signed=−4260, temperature=−4260/340+36.53=24.0005882353°C. Нульові gyro bytes дають 0°/с. UART показує −99 mg, 949 mg, 24000 m°C, бо integer formatter відкидає дробову частину, а не округлює її.

Переносимий fixture містить той самий frame, source hash, offset та signed counts. tools/check_capture_fixture.py повторно декодує уривок, C test викликає реальний MPU driver з fake transport, незалежний sigrok -B i2c=data-read звірив усі 14 bytes.

### NACK та реальна інтеграційна аномалія

probe nack звернувся до 0x69: wire D2=11010010, дев'ятий SDA HIGH, STOP, без register/data. У UART між двома однаковими SAMPLE t=184799 і t=184807 є PROBE t=184806 address=69 result=ERROR. Робоча адреса 0x68 не змінювалася. Адресний NACK є помилкою, останній master NACK нормального read є правилом протоколу.

Початкова HAL Mem_Read дала D0 ACK, 75 ACK без repeated START. Тимчасовий diagnostic ISR=00008043, CR2=020120D0, HAL error=00000020. Split HAL прочитав ID=68, але конфігураційний write зупинився на TXIS. У 633422f власний register adapter використовує TXE/TC/RXNE, explicit clearing START, AUTOEND і bounded abort. Після виправлення з'явилися ID, writes і 14-byte bursts. Це доказ HAL/model sequencing mismatch; внутрішню першопричину закритої моделі не доведено.

## Таймінги й цифрові сигнали

SCL median active period 10000 ns, high/low по 5000 ns, тобто 100 kHz. ADC poll interval близько 100 мс. Ширина ADC_MARK 71.425 мкс охоплює HAL_Start/Poll/GetValue/Stop. Теоретичні 14.4167 мкс sampling+conversion за 12 MHz не видаються за виміряну ширину.

light-activity PIR rises 26.524415225 і 85.109419175 с, кожний HIGH 5.000000025 с. LED rises 26.541936100 та 85.123721125 с. GPIO→LED 17.520875 і 14.301950 мс, нижче 100 мс PRD. LED off через 29.99678185 і 29.9965506 с після falling PIR. Software last HIGH прочитано polling, тому різниця до фізичного falling допускає до 20 мс.

Повторна активність: rises 136.087565850 і 158.895885675 с, останній fall 163.895885700 с. LED тримається від 136.094647450 до 193.876400150 с. Затримка після останнього fall 29.980514450 с. Другий HIGH продовжив інтервал. GPIO excerpt pir-activity показує обидва HIGH та LED.

LED при tilt у 60..65 с має median edge interval 249.069825 мс; при fault у 172..179 с 100.0079 мс. Firmware toggles прив'язані до tick, jitter містить scheduler/bus overhead. Період повного blink удвічі більший.

UART 115200 8N1, start LOW, 8 data LSB first, stop HIGH. Літера S=0x53 має data bits 11001010 у wire order; PulseView показує початок SAMPLE. Командна затримка виміряна від кінця CR/LF у D6 до першого response start у D2. У tilt run максимум 64.340 мс для pause imu on, інші 1.472..63.211 мс; межа PRD 200 мс виконується для записаних команд.

## Світло, нахил, zero й відновлення

LDR 500 модельних lux дав raw=1001, 0.1 lux дав 4063, 79.43282 lux дав avg=2210. При 2210 після темряви dark=1, після 100000 lux та повернення dark=0. В idle темрява не дає need_light, в active дає. Analog slider timestamp не записаний, тому точний час реакції від UI зміни світла не виміряно. Межу п'яти samples і 700 мс перевіряють host schedule scenarios.

MPU X=0.35 Z=1 дає 19.289° і tilt=0; X=0.35 Z=0.95 дає 20.225° і tilt=1 після витримки; X=0.30 Z=0.95 дає 17.526°, попередній tilt лишається; X=0.25 Z=0.95 дає 14.744° та clear після 500 мс. Точні 20/15° і 499/500 мс належать host vectors. X=0.35 Z=−0.05 має norm≈0.354 g та invalid без нової fault.

mode adjust t=65822, zero t=65826 rejected; zero t=99239 accepted після стабільного спостереження; auto t=99243 зберіг еталон. Вектор X=−0.1 Z=0.95 після zero дав 20.752°, restart цього самого вектора повернув кут 6.008° до +Z. Reset перевірено через restart simulator, не клік B2.

При PIR t≈162378 одночасні need_light=1, tilt=1. fault imu on t=171132 дав fault=1 та imu invalid, зберіг обидва прапорці; LED перейшов на 100 мс. ADC fault on плюс IMU off: IMU відновився за два samples t=180705, ADC fault лишився. Після ADC off fault=0 t=189205, light_valid ще 0; на п'ятому sample t=189505 light_valid=1.

pause imu/adc зупинив update. Останні samples 202105/202101 мс, invalid t=202606/202602, age=501 мс. zero t=214358 відхилено для stale. Після resume два успіхи t=214505 зняли fault, п'ять ADC samples t=214805 відновили світло.

## Матриця PRD

| Тест | Host доказ | Реальна симуляція / межі |
|---|---|---|
| T01 | reset, +Z, порожні windows | ready t=1, self-test, light_valid t=505; restart очистив zero |
| T02 | 30000 boundary, кожний HIGH, wrap | 5 с HIGH, 14.30/17.52 мс реакція, два impulses продовжили LED |
| T03 | 0/4095, 2399/2400/2001/2000, 5 samples | raw 32/1001/2210/4063; точні ADC boundary не задавалися UI |
| T04 | avg hysteresis і bounded schedule | avg2210 обидва збережені dark; analog reaction timestamp відсутній |
| T05 | 19/20/21/15/16°, 499/500 мс | 19.289/20.225/17.526/14.744°, tilt/set/clear |
| T06 | norm0.9/1.1/1.5, короткий invalid, gyro separation | norm0.354g invalid; окремий gyro rotation scenario не записаний |
| T07 | stable/unstable zero, overflow/unknown, modes | rejected/accepted/stale rejected; записані replies ≤64.34 мс |
| T08 | усі flags і LED priority | active+dark+tilt+fault, LED250/100 мс |
| T09 | ERROR/TIMEOUT/500/501, незалежні два успіхи | явна error injection, pause501, двоканальне recovery; фізичного від'єднання немає |
| T10 | wrap і concurrent events/commands | три сенсори весь run, команди й diagnostics при sampling; не hardware stress benchmark |

Жодний критерій електричної точності, струму, noise чи механічного кріплення не перевірено. Висновки стосуються даних моделі й виконаних host vectors.

## Власний внесок

Усі шість напрямів виконав Єрмак Олександр Віталійович, ІО-32. Команди не вигадано.

| Напрям | Власний результат |
|---|---|
| Звіт і документація | Звіт, analysis, журнал проблем |
| Документація і pinout | DS/RM/UM, рівні/AF, morpho, power budget, alternatives |
| Робота з апаратурою | Form B Wokwi wiring, self-test, captures; фізичного монтажу немає |
| Декодування і аналіз | I²C/UART/GPIO/ADC_MARK, manual bits, sigrok, NACK, timing |
| Структура проєкту і збірка | Core/Board/Desk/Sensors/Drivers, contracts, Docker/Make/CI |
| Тестування | Fake transport, mock expectations, captured fixture, boundaries, coverage, sanitizers |

Author/committer dates призначено на 25.09–12.10.2026 +03:00. Реальні записи та журнали лишаються датовані 13.10.2026. Старі CI screenshots/JSON є історичними; новий фінальний SHA перевіряється окремим run.
