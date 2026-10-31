# Сценарії ЛР3

Запускати лише у власному дозволеному GUI-сеансі або через доступну офіційну Wokwi CLI ліцензію. Готовий YAML не означає виконаний тест. CLI token не налаштований, trial не активовано.

`python3 tools/select_stimulus.py product` відновлює схему з реальним компонентом PIR моделі. `periodic` замінює тільки джерело PB0 на Wokwi clock generator 0.1425923285 Hz, період 7.013 с, HIGH половину періоду. PIR лишається намальованим і живленим, але його OUT від'єднано. Це повторюваний digital stimulus для EXTI/latency, а не калібрування PIR. MCU/прошивка/лінії сенсорів незмінні. У цій схемі LDR=0.1 модельного lux. Після досліду повернути product. Обидва diagram hashes мають бути записані в manifest.

`periodic-no-uart-probes` додатково від'єднує від логічного аналізатора тільки D2 (TX) і D6 (RX), щоб довгі UART-експорти не вичерпували 8 млн відліків. Сам UART і його журнал залишаються під'єднаними. Такий VCD не придатний для вимірювання часу на проводі UART; його призначення — зіставити D3, D7 та D1. Фактичний hash фіксує manifest.

Після старту Wokwi, з кореня репозиторію:

```sh
PYTHONPATH=../../../Lab03/Work/python-packages python3 tools/record_session.py S1 --seconds 210 --output docs/metrics/lab3/S1-run-01
PYTHONPATH=../../../Lab03/Work/python-packages python3 tools/record_session.py S2 --seconds 3600 --output docs/metrics/lab3/S2-run-01
PYTHONPATH=../../../Lab03/Work/python-packages python3 tools/record_session.py S3 --seconds 700 --command-rate 1 --output docs/metrics/lab3/S3-run-01
PYTHONPATH=../../../Lab03/Work/python-packages python3 tools/record_session.py fault --seconds 30 --output docs/metrics/lab3/fault-run-01
python3 tools/analyze_metrics.py docs/metrics/lab3/S3-run-01/uart.raw --output docs/metrics/lab3/S3-run-01/analysis.json
```

Кожний output має бути новим. S1 потребує >=210 с MCU і фактично >=2000 виміряних викликів кожної з задач ADC і IMU; нормальний період 100 мс дає номінально 2100 за 210 с. Якщо фактичних викликів менше, цей запуск недостатній. S2 потребує >=3600 с MCU. Початок і кінець тривалості визначають STATUS t у MCU, не host clock. Скрипт фіксує SHA256 ELF/HEX/BIN/diagram/config/скрипта до з'єднання. При timeout повертає incomplete. S2 кожні 300 с MCU виконує mode adjust, через 5 с zero, ще через 5 с mode auto; окремо journal кожні 120 с. Команди є запитами; перевіряти фактичні REPLY та умови zero в UART. Hash на диску сам по собі не доводить, яку схему завантажила наявна вкладка: після зміни diagram закрити вкладку Wokwi, запустити симулятор знову та перевірити сигнали. Host scheduling команд не є гарантією UART wire frequency, перевірити D6 в окремому короткому повному capture, якщо UART-проби відключені для довгого S3.

S1 product без подій, S2/S3 periodic або 30+ окремих ручних рухів з product. Перед S3 у Wokwi задати IMU X=0.55,Z=0.85 та перевірити tilt=1 у UART; це значення фактично працювало в попередньому неповному запуску. Скрипт чекає 65 с на 64 справжні секундні зрізи, потім викликає history та mode auto з конфліктним потоком. S3 вести з `periodic-no-uart-probes`: раніше повний UART на D2/D6 заповнив 8 млн відліків до кінця досліду. Після завершення перевірити повноту VCD та запуску й зіставити саме EXPORT (D1 також позначає flash):

```sh
python3 tools/validate_s3_events.py docs/metrics/lab3/S3-run-01/uart.raw docs/captures/lab3/S3-run-01.vcd --output docs/metrics/lab3/S3-run-01/export-events.json
```

Для заявленої метрики потрібно >=30 різних підтверджених входів під час повних EXPORT; не сумувати різні запуски як один. Непідтверджені, неоднозначні, обрізані та неповні записи не враховувати. Нічого не домальовувати.

Boundary: окремі runs `boundary --seconds 100 --command-rate 1`, потім 2,5,10,20,50. Сумарний потік обмежений serial 115200. Стартовий 65-секундний інтервал не включати в steady stress window. Стоп у Wokwi експортує VCD; відразу зберегти до відповідної run-папки, перевірити повноту й hash. Не перезаписувати VCD наступним дослідом.

Перевірка живлення та flash: save, journal, MCU reset з flash збереженою, journal. Зберегти flash dump перед і після. Перезапуск усієї симуляції може перезавантажувати образ, тому не доводить persistence. Випадкове закриття вкладки не називати power-cut test. Фізичного стенда немає.
