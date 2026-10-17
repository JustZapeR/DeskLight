# Збірка, симуляція й аналіз

Команди виконувати в корені DeskLight. Docker daemon має бути доступний. Host compiler повинен підтримувати C11.

```sh
make build
make test
make coverage
make sanitize
shasum -a 256 firmware/build/firmware.elf firmware/build/firmware.hex
```

У Linux для хешів використати sha256sum. make clean видаляє тільки firmware/build. make build збирає Docker image і ARM прошивку; ELF містить debug information, HEX/BIN завантажуваний код. Docker фіксує base digest і основні package versions, але apt helpers не прив'язані до snapshot. До Wokwi потрібний актуальний build, він [не компілює firmware сам](https://docs.wokwi.com/vscode/project-config).

## Wokwi

Відкрити корінь DeskLight у VS Code з офіційним Wokwi extension та доступною ліцензією. F1, Wokwi: Start Simulator. Очікувати DeskLight ready, SELFTEST id=68 result=OK і STATUS. Після п'ятого ADC sample light_valid=1. Початковий кут відносно +Z.

1. LDR поставити 0.1 lux, дочекатися avg=4063, dark=1. Без руху mode=idle, need_light=0.
2. PIR, Simulate motion. mode=active і need_light=1. HIGH триває 5 с, DeskLight продовжує активність 30 с від останнього прочитаного HIGH.
3. LDR 79.43282 lux дає avg=2210. Після dark прапорець dark=1, після яскравого 100000 lux і повернення до 79.43282 він dark=0.
4. MPU accelX=0.35, accelY=0, accelZ=0.95 g дає близько 20.225°. Після 500 мс tilt=1, LED перемикається кожні 250 мс.
5. Надіслати mode adjust, відразу zero, отримати zero rejected. Зберегти вектор валідним не менше 1 с, zero приймається. mode auto очищає активність і зберігає еталон.
6. sample читає WHO_AM_I й 14 bytes. probe nack перевіряє відсутню 0x69; повторний sample має працювати.
7. fault imu on/off, fault adc on/off і pause imu/adc on/off є явною діагностикою. Вони перевіряють error/timeout та stale без фізичного від'єднання.

Команди надсилати як ASCII-рядки з CR або LF. Довжина до 39 символів. Невідома команда дає unknown command, переповнення відхиляє весь рядок. При native paste перевірити реальний RX, розкладка клавіатури може підміняти літери. Клік по slider Wokwi змінює його значення; після кліку звірити число.

При зупинці Wokwi експортує wokwi.vcd. Негайно зберегти під окремим ім'ям, щоб наступний run його не перезаписав. Первинні повні записи збережено у Lab02/Work/captures; короткі перевірені уривки версіоновано у docs/captures. Restart simulator перевірено як очищення еталона. Дія кліку B2 у Wokwi не підтверджена.

## PulseView / sigrok

Відкрити docs/captures/captured-burst.sr у PulseView, налаштування captured-burst.pvs. I²C decoder: SCL D0, SDA D1. UART: TX D2, RX D6, 115200, 8N1, LSB first. GPIO sessions мають лише потрібні канали.

```sh
python3 tools/inspect_capture.py path/to/full.vcd --output path/to/decoded/capture
sigrok-cli -i docs/captures/captured-burst.sr -P i2c:scl=D0:sda=D1 -B i2c=data-read
sigrok-cli -i docs/captures/sample-nack.sr -P uart:tx=D2:rx=D6:baudrate=115200 -B uart=tx
```

Binary decoder виводить raw bytes; спрямувати у файл для hex або порівняння. Під час VCD import у sigrok: downsample=100 для 10 MHz regular samples. 1 ns у VCD є одиницею timestamp, не фізичною частотою sampling. Довгі slow GPIO records мають меншу SR частоту; I²C на них не декодувати. Межі, SHA256 і offsets описано в [аналізі](captures/analysis.md).
