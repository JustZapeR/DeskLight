#!/usr/bin/env python3
"""Check portable includes and the DeskLight pin contract before compilation."""
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
for layer in ("Desk", "Sensors"):
    for path in (root / "firmware" / layer).glob("*.[ch]"):
        source = path.read_text()
        assert not re.search(r"\b(?:HAL_\w+|GPIO\w*|USART\w*|I2C_HandleTypeDef)\b", source), path
        for include in re.findall(r'#include\s+"([^"]+)"', source):
            target = (path.parent / include).resolve()
            assert target.is_relative_to(root / "firmware" / layer), (path, include)
print("PASS architecture: Desk and Sensors cannot include HAL, Core, Board or each other")
d = json.loads((root / "diagram.json").read_text())
wires = {frozenset(c[:2]) for c in d["connections"]}
for source, target in {
    "pir:OUT": "nucleo:PB0", "ldr:AO": "nucleo:PA0",
    "imu:SCL": "nucleo:PB8", "imu:SDA": "nucleo:PB9",
    "imu:AD0": "nucleo:GND.1", "imu:VCC": "nucleo:3V3.1",
    "ldr:VCC": "nucleo:3V3.1", "pir:VCC": "nucleo:5V.1",
    "$serialMonitor:RX": "nucleo:PA2", "$serialMonitor:TX": "nucleo:PA3",
    "logic:D4": "nucleo:PA4", "logic:D5": "nucleo:PA5",
}.items():
    assert frozenset((source, target)) in wires, (source, target)
ioc = (root / "firmware/firmware.ioc").read_text()
for setting in ("PA0.Signal=ADC1_IN0", "PB0.Signal=GPIO_Input", "PA4.Signal=GPIO_Output",
                "PB8.Signal=I2C1_SCL", "PB9.Signal=I2C1_SDA",
                "ADC1.ClockPrescaler=ADC_CLOCK_SYNC_PCLK_DIV4",
                "ADC1.SelectedChannel=ADC_CHANNEL_0", "ADC1.NbrOfConversion=1",
                "ADC1.Resolution=ADC_RESOLUTION_12B",
                "ADC1.SamplingTimeCommon1=ADC_SAMPLETIME_160CYCLES_5"):
    assert setting in ioc, setting
print("PASS wiring: sensors, voltage rails, analyzer and IOC agree")
