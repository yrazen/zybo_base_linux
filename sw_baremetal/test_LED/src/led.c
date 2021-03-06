/*
 * led.c
 *
 *  Created on: 26.09.2014
 *      Author: phil
 */


#include <xparameters.h>
#include <xgpio.h>
#include <xil_printf.h>
#include <stdio.h>

#define LED_DEVICE_ID	XPAR_LEDS_4BITS_DEVICE_ID
#define LED_BASEADDR	XPAR_LEDS_4BITS_BASEADDR
#define LED_DELAY		10000000UL

int main(void) {
	XGpio *led_inst;
	XGpio_Config *led_config;
	u8 data = 0;

	led_config = XGpio_LookupConfig(LED_DEVICE_ID);
	XGpio_CfgInitialize(led_inst, led_config, led_config->BaseAddress);
	XGpio_SetDataDirection(led_inst, 1, 0);	// auf ausgang setzen

	printf("LED initialisation done\n");

	while(1) {
		XGpio_DiscreteWrite(led_inst, 1, data);
		for (volatile int i = 0; i < LED_DELAY; i++);
		//(data < 0xf) ? data++ : data--;
		data = (data+1) % 0x10;
	}

}
