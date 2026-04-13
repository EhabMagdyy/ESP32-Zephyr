#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart2));

#define STACK_SIZE 		1024
#define LED_PRIORITY 	7
#define UART_PRIORITY 	6

static const struct gpio_dt_spec thd1_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);


// Allocates memory for a thread stack at compile time and gives
K_THREAD_STACK_DEFINE(ledthrthread_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(uartthrthread_stack, STACK_SIZE);

struct k_thread thread_led2;
struct k_thread thread_uart2;

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

void uart2_thread(void *arg1, void *arg2, void *arg3){
	char msg[] = "Hello from ESP32!\r\n";

	while(1){
		for (int i = 0; i < sizeof(msg) - 1; i++) {
			uart_poll_out(uart_dev, msg[i]);
		}
		printk("UART Thread\n");
		k_msleep(2000);
	}
}

int main(void){
	// Ensures the GPIO controller is initialized
	if(!gpio_is_ready_dt(&thd1_led)){
		printk("GPIO not ready!\n");
        return 0;
    }
	printk("GPIO ready!\n");
	
	if(!device_is_ready(uart_dev)){
		printk("UART not ready!\n");
		return 0;
	}
	printk("UART2 ready!\n");

	
	// Create threads
	// params: thread control block struct - thread stack handle - stack size - thread function - function parameters (3) - priority - options - delay (start immediately)
    k_thread_create(&thread_led2, ledthrthread_stack, STACK_SIZE, led2_thread, NULL, NULL, NULL, LED_PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&thread_uart2, uartthrthread_stack, STACK_SIZE, uart2_thread, NULL, NULL, NULL, UART_PRIORITY, 0, K_NO_WAIT);

	while(1){
		// make this thread sleeps forever
		k_sleep(K_FOREVER);
	}
	return 0;
}
