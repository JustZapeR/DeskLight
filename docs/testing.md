# Перевірки DeskLight

Host tests виконують Sensors та Desk без HAL/CMSIS. Fake bus повертає контрольований WHO_AM_I, raw frame, ERROR і TIMEOUT. Перевірка порядку записів є mock expectation над fake transport; після невдалого читання sample і raw buffer не змінюються.

| Група | Вимоги | Фактично перевірено на хості |
|---|---|---|
| driver | FR-04, FR-07/08, FR-12 | ID, п'ять конфігураційних записів, відмови на кожному кроці, signed big-endian, g, °/с, температура |
| desk | FR-01..06, FR-09, FR-13 | Заповнення 5 відліків, середнє, 2399/2400/2001/2000, 0/4095, HIGH на кожному poll, продовження 30 с, adjust/auto |
| tilt | FR-07..09, FR-12/13 | 19/20/21/15/16°, 499/500 мс, 0.9/1.1 g і значення за межами, invalid, стабільність, zero, +Z після reset, tick wrap |
| console | FR-09..11, FR-13 | Рядки UART, CRLF, unknown/overflow, усі прапорці одночасно, LED 100/250 мс і steady HIGH |
| faults | FR-01..13 | Одночасні прапорці, незалежне відновлення за 2 успіхи, 500/501 мс, avg 2399.8, restart і команди під час опитування |

Команди: `make test`, `make sanitize`, `make coverage`. Перевірки пройшли 13.10.2026 на macOS arm64 clang та Linux aarch64 GCC у Docker. AddressSanitizer/UndefinedBehaviorSanitizer на macOS пройшли без діагностик. Логи і машинні результати у docs/validation.

## Покриття переносимого коду

Обраний поріг ≥90% рядків і ≥80% напрямків гілок. Він вимагає виконання основних станів та error paths; граничні сценарії перевіряються окремими asserts, бо високий відсоток сам по собі не доводить правильність правил.

| Модуль | GCC рядки | GCC branch taken | clang рядки | clang branch taken |
|---|---|---|---|---|
| Sensors/imu.c | 100% / 27 | 100% / 18 | 100% / 31 | 100% / 20 |
| Desk/desk.c | 100% / 98 | 97.12% / 104 | 100% / 107 | 94.68% / 94 |
| Desk/console.c | 100% / 44 | 94.44% / 54 | 100% / 43 | 85.19% / 54 |

GCC та clang рахують виконувані рядки й гілки по-різному. Core, Board та vendor code не входять до цих відсотків. ARM build перевіряє компонування, host tests перевіряють алгоритми; тільки Wokwi recordings можуть підтвердити runtime обмін, GPIO, розклад і затримки UART у симуляції.

Для відтворення Linux host run використовувався окремий локальний Docker image з Ubuntu 22.04, GCC 11, libc6-dev, make і Python 3. Каталоги build/coverage розділені за OS/arch, щоб Linux не запускав Mach-O з macOS.

## Записані байти замість лише синтетичних даних

tests/fixtures/mpu-captured.json містить raw burst з реального Wokwi VCD, timestamp і SHA256 джерела/уривка. Драйверний тест використовує той самий frame через fake transport. tools/check_capture_fixture.py повторно читає уривок, звіряє всі 14 байтів і останній master NACK. Незалежний sigrok decoder повернув ті самі байти. Цей тест поєднує реальний вхід із переносимим driver execution; він не замінює MCU adapter test.

[Матриця T01..T10](captures/analysis.md#матриця-prd) відрізняє точні хостові межі від сценаріїв симулятора. CI збирає ARM і виконує test, coverage та ASan/UBSan. Перевірений run після runtime виправлення: [38439355461](https://github.com/JustZapeR/DeskLight/actions/runs/38439355461). GitHub попереджає що pinned checkout action декларує Node 20 і runner примусово використовує Node 24; виконання job успішне.
