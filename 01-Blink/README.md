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
``` bash
sudo apt install screen
screen /dev/ttyUSB0 115200
# to terminate
# ctrl + A + X
# from another terminal run if it
sudo pkill -9 -f "screen dev/ttyUSB0 115200"
```