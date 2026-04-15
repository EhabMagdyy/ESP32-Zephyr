# Zephyr Multi-Threaded LED Blink for ESP32

## Source python environment
``` bash
source /home/ehab/zephyrproject/zephyr/.venv/bin/activate
```

## Build & give it an overlay
``` bash
west build -p always -b esp32_devkitc/esp32/procpu . --extra-dtc-overlay board/esp32.overlay -DPython3_EXECUTABLE=/home/ehab/zephyrproject/.venv/bin/python3
```

## Flash
``` bash
west flash --esp-device /dev/ttyUSB0
```

## Monitor
``` sh
pip install esp-idf-monitor
python -m esp_idf_monitor --port /dev/ttyUSB0 --baud 115200 build/zephyr/zephyr.elf
# to terminate
# ctrl + ]
```

