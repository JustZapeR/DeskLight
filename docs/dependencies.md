# Залежності

Основу Core, CMSIS та ST HAL 1.0.1 збережено з коміту ЛР1 a1dc82a. Додані ADC/ADCEx та LL ADC headers взято без змін зі STMicroelectronics/stm32c0xx-hal-driver v1.0.1, commit 24999b605d0b9da3e4675c4c884036ae724bb59f. SHA256 у validation/vendor-adc.json. Ліцензії ST/CMSIS залишені у Drivers.

ADC потрібен LDR; I²C MPU6050; UART журналу й командам; GPIO PIR/LED/ADC_MARK; RCC/FLASH/PWR/CORTEX startup та SysTick. DMA типи залишаються залежністю API I²C/UART, DMA transfers не викликаються. EXTI залишений у базовому Core для B1, продуктової дії B1 немає.

Власні переносимі модулі використовують C11, стандартну бібліотеку й libm. Стороннього тестового framework немає. assert дозволяє тестувати без додаткових залежностей, але не дає автоматичної генерації mocks та докладних assertion reports.
