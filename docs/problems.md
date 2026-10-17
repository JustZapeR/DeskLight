# Журнал фактичних проблем

| Проблема та доказ | Причина / рівень впевненості | Виправлення |
|---|---|---|
| ADC не був у базовому HAL/Makefile ЛР1 | Факт за джерелами: модуль вимкнений і файли відсутні | [b50c216](https://github.com/JustZapeR/DeskLight/commit/b50c216): офіційні ADC/ADCEx/LL v1.0.1, init і build |
| Перший PIR HIGH губився, поки health ще invalid | Host assert виявив очищення нового last_motion під час стартового health стану | [f8a6da5](https://github.com/JustZapeR/DeskLight/commit/f8a6da5): зберігати HIGH незалежно від початкового health |
| Після тривалої паузи LDR міг використати попередній average | Host stale/recovery перевірка відокремила health від готовності 5 samples | [dc635eb](https://github.com/JustZapeR/DeskLight/commit/dc635eb): скидати window після gap, чекати 5 нових |
| Один tilt test очікував clear після 400 мс | Помилка тесту, не firmware: критерій PRD має 500 мс | [dc635eb](https://github.com/JustZapeR/DeskLight/commit/dc635eb): перевірки 499/500 мс |
| NaN у console numeric formatter спричиняв UBSan float-to-int diagnostic | Before log у validation/nonfinite-before.log, conversion без finite guard | [633422f](https://github.com/JustZapeR/DeskLight/commit/633422f): finite/range guard, after log без diagnostic |
| HAL Mem_Read мав ACK адреси й регістру, але не repeated START; split HAL write зависав на TXIS | Спостережена HAL/model sequencing mismatch; внутрішню причину закритої моделі не доведено | [633422f](https://github.com/JustZapeR/DeskLight/commit/633422f): Board register adapter, RXNE/TXE/TC, explicit START, bounded STOP; before/after VCD |
| UART PA2 був помилково віднесений до CN10.35 | Помилка pinout документа, звірена з UM2953 table 11 | [633422f](https://github.com/JustZapeR/DeskLight/commit/633422f): PA2 CN7.35, PA3 CN7.37 |

Timestamps записів є фактичними. Author/committer dates призначено на 25.09–12.10.2026 +03:00 і не є часом виконання. Тимчасові I²C діагностики не лишилися в продуктовому журналі. Error injection позначено DIAGNOSTIC і не видається за фізичне від'єднання.
