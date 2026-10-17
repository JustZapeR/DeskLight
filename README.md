# DeskLight

Єрмак Олександр Віталійович, ІО-32.

DeskLight показує потребу додати світло після недавнього руху біля столу та незалежно контролює нахил основи лампи. PIR, аналоговий LDR і MPU6050 працюють одночасно на STM32C031C6. UART передає дані й приймає команди, LED має пріоритет fault > tilt > need_light. Силового керування немає.

ЛР2 реалізована у формі B, Wokwi. Реальні VCD, PulseView sessions, decoded bytes та вимірювання містить [аналіз записів](docs/captures/analysis.md). Точні алгоритмічні межі, помилки, відновлення і tick wrap перевірено на хості. LDR не відкалібрований у люксах, PIR не доводить нерухому присутність.

## Відтворення

У корені цієї єдиної робочої копії:

```sh
make build
make test
make coverage
make sanitize
```

Docker потрібний для ARM build. Host tests потребують C11 compiler, make і Python 3; coverage додатково gcov або llvm-cov. ARM container використовує закріплений Ubuntu 22.04 і GCC 10.3, перевірки виконано на macOS arm64 clang, Linux aarch64 GCC 11 та в GitHub Actions. [Інструкція Wokwi й PulseView](docs/run.md).

## Документи

- [Вимоги FR-01..13 і T01..10](docs/PRD.md)
- [Піни, рівні, живлення й альтернативи](docs/pinout.md)
- [Архітектура й runtime adapter](docs/architecture.md)
- [Тести, fixtures та coverage](docs/testing.md)
- [Журнал проблем із fix commits](docs/problems.md)
- [Виконання етапів](docs/roadmap.md)
- [Звіт](docs/lab_2/)

main містить злиту ЛР1. ЛР2 продовжує її у feature/lab-2. Історичні [збірки ЛР1](docs/checks/results.md) не описують поточну прошивку. Ліцензії основи, ST та CMSIS збережено. Основа [STM32 HAL Wokwi example](https://github.com/wokwi/stm32-hello-wokwi).
