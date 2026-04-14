# ESP32 Zephyr UART Pooling Demo

Three threads run concurrently: one blinks an LED, one transmits over UART2, one receives and prints.

---

## Device Handle

```c
const struct device *uart2_dev = DEVICE_DT_GET(DT_NODELABEL(uart2));
```

`DEVICE_DT_GET` resolves the `uart2` node from the device tree at compile time and stores a pointer to the driver instance. This is how Zephyr avoids hardcoded register addresses.

---

## Configuration Constants

```c
#define STACK_SIZE       1024   // bytes per thread stack
#define LED_PRIORITY     7      // lower number = higher priority
#define UART_TX_PRIORITY 6
#define UART_RX_PRIORITY 6
#define MSG_SIZE         19     // "Hello from ESP32!\r\n" = exactly 19 bytes
```

TX and RX share the same priority (6), which is **higher than the LED thread (7)**. `MSG_SIZE` is set to 19 to match the TX message length exactly, cause im connecting tx to rx.

---

## Thread Stacks & Control Blocks

```c
K_THREAD_STACK_DEFINE(led_stack,      STACK_SIZE);
K_THREAD_STACK_DEFINE(uart_tx_stack,  STACK_SIZE);
K_THREAD_STACK_DEFINE(uart_rx_stack,  STACK_SIZE);

struct k_thread thread_led2;
struct k_thread thread_uart2_tx;
struct k_thread thread_uart2_rx;
```

`K_THREAD_STACK_DEFINE` allocates **1 KB of stack memory at compile time** for each thread. The `k_thread` structs are the control blocks Zephyr uses internally to schedule and manage each thread.

---

## Thread 1 — LED Blink

```c
void led2_thread(void *arg1, void *arg2, void *arg3){
    gpio_pin_configure_dt(&thd1_led, GPIO_OUTPUT_ACTIVE);
    while(1){
        gpio_pin_toggle_dt(&thd1_led);
        printk("LED Thread\n");
        k_msleep(1000);         // yields CPU for 1 second
    }
}
```

Configures the GPIO pin once, then toggles it every second. `k_msleep` yields the CPU to the Zephyr scheduler — the LED thread is not busy-waiting.

---

## Thread 2 — UART TX (Polling)

```c
void uart2_tx_thread(void *arg1, void *arg2, void *arg3){
    char msg[] = "Hello from ESP32!\r\n";   // 19 bytes
    while(1){
        for(int i = 0; i < sizeof(msg) - 1; i++){
            uart_poll_out(uart2_dev, msg[i]);   // blocking, one byte at a time
        }
        printk("UART2 TX Thread\n");
        k_msleep(2000);
    }
}
```

`uart_poll_out` is **blocking per byte** — it waits until the TX FIFO can accept each character before returning. The `-1` skips the null terminator. Every 2 seconds a full message is sent.

---

## Thread 3 — UART RX (Polling)

```c
void uart2_rx_thread(void *arg1, void *arg2, void *arg3){
    uint8_t data[MSG_SIZE];
    while(1){
        for(uint8_t idx = 0; idx < MSG_SIZE; idx++){
            unsigned char ch;
            while(uart_poll_in(uart2_dev, &ch) != 0){
                k_msleep(1);    // yield CPU while waiting for a byte
            }
            data[idx] = ch;
        }
        printk("Received %d bytes: ", MSG_SIZE);
        for(int i = 0; i < MSG_SIZE; i++) printk("%c", data[i]);
        printk("\n");
    }
}
```

`uart_poll_in` is **non-blocking** — it returns `-1` immediately if no byte is available. The inner `while` loop retries every 1ms, calling `k_msleep(1)` each time to yield the CPU instead of spinning. Once all `MSG_SIZE` bytes are collected, the full message is printed.

> `k_msleep(1)` inside the polling loop is critical — without it this thread would starve all lower-priority threads.

---

## main() — Initialization & Thread Creation

```c
int main(void){
    // 1. Verify hardware is ready
    if(!gpio_is_ready_dt(&thd1_led))  { printk("GPIO not ready!\n");  return 0; }
    if(!device_is_ready(uart2_dev))   { printk("UART2 not ready!\n"); return 0; }

    // 2. Spawn all three threads immediately (K_NO_WAIT)
    k_thread_create(&thread_led2,     led_stack,     STACK_SIZE, led2_thread,     ...);
    k_thread_create(&thread_uart2_tx, uart_tx_stack, STACK_SIZE, uart2_tx_thread, ...);
    k_thread_create(&thread_uart2_rx, uart_rx_stack, STACK_SIZE, uart2_rx_thread, ...);

    // 3. Park main thread forever — nothing left to do
    while(1){ k_sleep(K_FOREVER); }
}
```

`main()` is itself a Zephyr thread. After creating the three workers it parks itself with `k_sleep(K_FOREVER)` — it stays alive (required) but consumes no CPU.

---

## Data Flow (Loopback)

```
uart2_tx_thread                uart2_rx_thread
───────────────                ───────────────
"Hello from ESP32!\r\n"  ──►  GPIO16 (RX2) ◄── GPIO17 (TX2)
uart_poll_out() x19            uart_poll_in() loop
                               collects 19 bytes
                               printk("Received...")
```

With TX2 (GPIO17) wired to RX2 (GPIO16), every byte sent by the TX thread is immediately received by the RX thread.

---

## Thread Timeline

```
Time(s)   LED Thread       TX Thread             RX Thread
──────    ──────────       ─────────             ─────────
0         toggle+sleep     send 19 bytes         polling...
1         toggle+sleep     sleeping (2s)         polling...
2         toggle+sleep     send 19 bytes         got 19 bytes → print
3         toggle+sleep     sleeping (2s)         polling...
4         toggle+sleep     send 19 bytes         got 19 bytes → print
```

---

## Build & Flash for ESP32

### Build
``` bash
west build -p always -b esp32_devkitc/esp32/procpu . --extra-dtc-overlay board/esp32.overlay -DPython3_EXECUTABLE=/home/ehab/zephyrproject/.venv/bin/python3
```

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

### Output of screen UART0 (printk) (Connect Tx2 <-> Rx2 for testing)
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