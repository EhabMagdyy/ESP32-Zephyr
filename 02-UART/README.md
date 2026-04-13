# ESP32 Zephyr UART

## Build & Flash for ESP32

### Build
``` bash
west build -p always -b esp32_devkitc/esp32/procpu . --extra-dtc-overlay board/esp32.overlay -DPython3_EXECUTABLE=/home/ehab/zephyrproject/.venv/bin/python3

### Flash
``` bash
west flash --esp-device /dev/ttyUSB0
```

### Monitor
``` bash
sudo apt install screen
screen /dev/ttyUSB0 115200
# to terminate
# ctrl + A + X
# from another terminal run to kill if you cannot terminate it
sudo pkill -9 -f "screen dev/ttyUSB0 115200"
```

### Output of UART2
```
Hello from ESP32!
Hello from ESP32!
Hello from ESP32!
Hello from ESP32!
```

### Output of screen UART0 (printk)
```
LED Thread
UART Thread
LED Thread
LED Thread
UART Thread
LED Thread
LED Thread
UART Thread
```