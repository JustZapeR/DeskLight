# Збірка та запуск DeskLight

Виконувати команди в корені DeskLight. Потрібні Docker, make та доступ до Docker daemon без sudo. Для середовища Ubuntu 24.04 перевірити:

```sh
docker --version
docker info
make clean
make build
ls -lh firmware/build/firmware.elf firmware/build/firmware.hex firmware/build/firmware.bin
sha256sum firmware/build/firmware.elf firmware/build/firmware.hex firmware/build/firmware.bin
```

На macOS для хешів використати `shasum -a 256` з тими самими файлами. make build викликає Dockerfile, збірка прошивки виконується в /workspace контейнера. Поточний Dockerfile має ubuntu:22.04; це тулчейн, а Ubuntu 24.04 є системою виконання. make clean видаляє firmware/build, але зберігає джерела й документи. ELF містить код і debug information, HEX та BIN створюються arm-none-eabi-objcopy. Артефакти збірки не входять до Git.

## Wokwi

1. Відкрити саме корінь DeskLight у VS Code з розширенням Wokwi та вже доступною ліцензією. Новий trial для цього етапу не активується.
2. Перед запуском виконати make build. [Wokwi не збирає прошивку сам](https://docs.wokwi.com/vscode/project-config), а читає шляхи від wokwi.toml. У поточному файлі це firmware/build/firmware.hex та firmware/build/firmware.elf.
3. F1 → Wokwi: Start Simulator. Очікуваний результат за наявним кодом: `STM32C031C6 ready`, потім `tick 0`, `tick 1`, `tick 2` і далі, з можливим зсувом першого цілого значення залежно від часу старту.
4. Спостерігати LED PA5: перемикання приблизно раз на секунду, повний цикл близько двох секунд. На схемі є тільки Nucleo та UART-з'єднання, жодних датчиків.
5. Зберегти фактичний UART-лог і скриншот з LED та схемою. Поки запуск не виконано, очікуваний текст не вважається отриманим результатом.

Наявний код printf передає через USART2, але ще не обробляє команди. Стартове повідомлення й блимання підтверджують базовий UART/LED етап після реального запуску. Вони не перевіряють цільові вимоги датчиків із PRD.

## Докази збірки

[Результати та хеші](checks/results.md) містять дві чисті збірки macOS і одну Ubuntu, точний рівень перевірки та відмінність debug ELF між архітектурами тулчейну. Логи зберігають справжній час виконання. Документація запуску не замінює реальний Wokwi запис.
