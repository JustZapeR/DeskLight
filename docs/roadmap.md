# Виконання DeskLight

ЛР1 збережено у main після merge. Вона визначила власний PRD, компоненти й Docker foundation. Її журнали збірки є історичними.

## Завершені етапи ЛР2

1. Додано PIR GPIO, LDR ADC, MPU I²C, pull-ups, analyzer; узгоджено .ioc та офіційні ST ADC files.
2. Переносимий register driver читає ID, налаштовує MPU і декодує 14-byte signed burst.
3. Desk реалізує 30-секундний інтервал, rolling average 5 і light hysteresis 2400/2000.
4. Додано tilt 20/15°, 500 мс, norm validity, stable zero в adjust.
5. UART line parser, IRQ RX, TX queue, bounded scheduler та LED priority працюють одночасно з датчиками.
6. Host tests, fixtures з реально записаних байтів, coverage thresholds, sanitizers та ARM CI виконуються автоматично.
7. Wokwi записи підтвердили обмін і representative scenarios. PulseView відкрив ті самі дані, sigrok незалежно підтвердив fixture.
8. Звіт описує фактичні докази та межі симуляції.

Фізичний монтаж, силове керування, точний люксметр і охоронна сигналізація до цього етапу не входять.
