#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

const struct device *uart2_dev = DEVICE_DT_GET(DT_NODELABEL(uart2));

#define STACK_SIZE 			1024
#define LED_PRIORITY 		7
#define UART_TX_PRIORITY 	6
#define UART_RX_PRIORITY 	6

#define RX_BUF_SIZE 		64
#define MSG_SIZE    		19

uint8_t rx_buf[RX_BUF_SIZE];

static const struct gpio_dt_spec thd1_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);


// Allocates memory for a thread stack at compile time and gives
K_THREAD_STACK_DEFINE(led_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(uart_tx_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(uart_rx_stack, STACK_SIZE);


struct k_thread thread_led2;
struct k_thread thread_uart2_tx;
struct k_thread thread_uart2_rx;

void led2_thread(void *arg1, void *arg2, void *arg3){
	// Configure the pin as output and set initial state, active means active state (on if high)
	int ret = gpio_pin_configure_dt(&thd1_led, GPIO_OUTPUT_ACTIVE);
	if(ret < 0){
		return;
	}
	while(1){
		gpio_pin_toggle_dt(&thd1_led);
		printk("LED Thread\n");
		k_msleep(1000);
	}
}

void uart2_tx_thread(void *arg1, void *arg2, void *arg3){
	char msg[] = "Hello from ESP32!\r\n";

	while(1){
		for (int i = 0; i < sizeof(msg) - 1; i++) {
			uart_poll_out(uart2_dev, msg[i]);
		}
		printk("UART2 TX Thread\n");
		k_msleep(2000);
	}
}

void uart2_rx_thread(void *arg1, void *arg2, void *arg3){
    uint8_t data[MSG_SIZE];

    while(1){
        // BLOCK until MSG_SIZE bytes received
        for(uint8_t idx = 0; idx < MSG_SIZE; idx++){

            unsigned char ch;
            // wait until a byte arrives
            while(uart_poll_in(uart2_dev, &ch) != 0) {
                k_msleep(1);   // yield CPU (important)
            }
            data[idx] = ch;
        }
        printk("Received %d bytes: ", MSG_SIZE);

        for(int i = 0; i < MSG_SIZE; i++){
            printk("%c", data[i]);
        }

        printk("\n");
    }
}

int main(void){
	// Ensures the GPIO controller is initialized
	if(!gpio_is_ready_dt(&thd1_led)){
		printk("GPIO not ready!\n");
        return 0;
    }
	printk("GPIO ready!\n");
	
	if(!device_is_ready(uart2_dev)){
		printk("UART2 not ready!\n");
		return 0;
	}
	printk("UART2 ready!\n");

	
	// Create threads for LED blinking, UART transmission, and UART reception
    k_thread_create(&thread_led2, led_stack, STACK_SIZE, led2_thread, NULL, NULL, NULL, LED_PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&thread_uart2_tx, uart_tx_stack, STACK_SIZE, uart2_tx_thread, NULL, NULL, NULL, UART_TX_PRIORITY, 0, K_NO_WAIT);
	k_thread_create(&thread_uart2_rx, uart_rx_stack, STACK_SIZE, uart2_rx_thread, NULL, NULL, NULL, UART_RX_PRIORITY, 0, K_NO_WAIT);

	while(1){
		// make this thread sleeps forever
		k_sleep(K_FOREVER);
	}
	return 0;
}
