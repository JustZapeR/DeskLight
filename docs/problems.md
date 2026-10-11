# Журнал фактичних проблем

| Проблема та доказ | Причина / рівень впевненості | Виправлення |
|---|---|---|
| ADC не був у базовому HAL/Makefile ЛР1 | Факт за джерелами: модуль вимкнений і файли відсутні | [62f6bad](https://github.com/JustZapeR/DeskLight/commit/62f6bad): офіційні ADC/ADCEx/LL v1.0.1, init і build |
| Перший PIR HIGH губився, поки health ще invalid | Host assert виявив очищення нового last_motion під час стартового health стану | [cdb9397](https://github.com/JustZapeR/DeskLight/commit/cdb9397): зберігати HIGH незалежно від початкового health |
| Після тривалої паузи LDR міг використати попередній average | Host stale/recovery перевірка відокремила health від готовності 5 samples | [eebe40a](https://github.com/JustZapeR/DeskLight/commit/eebe40a): скидати window після gap, чекати 5 нових |
| Один tilt test очікував clear після 400 мс | Помилка тесту, не firmware: критерій PRD має 500 мс | [eebe40a](https://github.com/JustZapeR/DeskLight/commit/eebe40a): перевірки 499/500 мс |
| NaN у console numeric formatter спричиняв UBSan float-to-int diagnostic | Before log у validation/nonfinite-before.log, conversion без finite guard | [db8b54b](https://github.com/JustZapeR/DeskLight/commit/db8b54b): finite/range guard, after log без diagnostic |
| HAL Mem_Read мав ACK адреси й регістру, але не repeated START; split HAL write зависав на TXIS | Спостережена HAL/model sequencing mismatch; внутрішню причину закритої моделі не доведено | [db8b54b](https://github.com/JustZapeR/DeskLight/commit/db8b54b): Board register adapter, RXNE/TXE/TC, explicit START, bounded STOP; before/after VCD |
| UART PA2 був помилково віднесений до CN10.35 | Помилка pinout документа, звірена з UM2953 table 11 | [db8b54b](https://github.com/JustZapeR/DeskLight/commit/db8b54b): PA2 CN7.35, PA3 CN7.37 |

Усі timestamps і commit dates є фактичними. Тимчасові I²C діагностики не лишилися в продуктовому журналі. Error injection позначено DIAGNOSTIC і не видається за фізичне від'єднання.
