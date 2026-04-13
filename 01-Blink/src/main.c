#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define STACK_SIZE 		1024
#define PRIORITY 		7

// gpio_dt_spec bundles: GPIO port, pin number, flags (active high/low)
// GPIO_DT_SPEC_GET(...) extracts this info from DeviceTree
static const struct gpio_dt_spec thd1_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec thd2_led = GPIO_DT_SPEC_GET(DT_ALIAS(led4), gpios);
static const struct gpio_dt_spec thd3_led = GPIO_DT_SPEC_GET(DT_ALIAS(led5), gpios);


// Allocates memory for a thread stack at compile time and gives
K_THREAD_STACK_DEFINE(thread1_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread3_stack, STACK_SIZE);

struct k_thread thread1_led2;
struct k_thread thread2_led4;
struct k_thread thread3_led5;

void led2_thread(void *arg1, void *arg2, void *arg3){
	// Configure the pin as output and set initial state, active means active state (on if high)
	int ret = gpio_pin_configure_dt(&thd1_led, GPIO_OUTPUT_ACTIVE);
	if(ret < 0){
		return;
	}
	while(1){
		gpio_pin_toggle_dt(&thd1_led);
		printk("Thread 0\n");
		k_msleep(1000);
	}
}

void led4_thread(void *arg1, void *arg2, void *arg3){
	int ret = gpio_pin_configure_dt(&thd2_led, GPIO_OUTPUT_ACTIVE);
	if(ret < 0){
		return;
	}
	while(1){
		gpio_pin_toggle_dt(&thd2_led);
		printk("Thread 1\n");
		k_msleep(800);
	}
}

void led5_thread(void *arg1, void *arg2, void *arg3){
	int ret = gpio_pin_configure_dt(&thd3_led, GPIO_OUTPUT_ACTIVE);
	if(ret < 0){
		return;
	}
	while(1){
		gpio_pin_toggle_dt(&thd3_led);
		printk("Thread 2\n");
		k_msleep(1200);
	}
}

int main(void){
	// Ensures the GPIO controller is initialized
	if(!gpio_is_ready_dt(&thd1_led) || !gpio_is_ready_dt(&thd2_led) || !gpio_is_ready_dt(&thd3_led)){
		printk("GPIO not ready!\n");
        return 0;
    }
	printk("GPIO ready!\n");
	
	// Create threads
	// params: thread control block struct - thread stack handle - stack size - thread function - function parameters (3) - priority - options - delay (start immediately)
    k_thread_create(&thread1_led2, thread1_stack, STACK_SIZE, led2_thread, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&thread2_led4, thread2_stack, STACK_SIZE, led4_thread, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&thread3_led5, thread3_stack, STACK_SIZE, led5_thread, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);

	while(1){
		// make this thread sleeps forever - its good to leave main thread there
		k_sleep(K_FOREVER);
	}
	return 0;
}
