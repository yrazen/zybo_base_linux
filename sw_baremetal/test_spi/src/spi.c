/*
 * spi.c
 *
 *  Created on: 28.09.2014
 *      Author: phil
 */

#include <malloc.h>
#include <xparameters.h>
#include <xspips.h>
#include <xscugic.h>
#include <xgpio.h>
#include <xil_exception.h>
#include <xil_printf.h>

#define LED_DEVICE_ID		XPAR_LEDS_4BITS_DEVICE_ID
#define SPI_DEVICE_ID		XPAR_PS7_SPI_1_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_PS7_SCUGIC_0_DEVICE_ID

#define SPI_INTERRUPT_ID	XPAR_XSPIPS_1_INTR
#define SPI_PRIORITY		0xA0
#define SPI_TRIGGER			0x03	// rising edge

#define LED_MASK			0x0f

#define MAX_BUF_SIZE		3

static XGpio* led_inst;
static XSpiPs* spi_inst;
static XScuGic* intc_inst;

static u8 spi_status;

int setupInterruptSystem(void);

int setupGpio(void);

int setupSpi(u32 prescale, u32 cpha, u32 cpol);

void spiISR(void* instancePtr);

static inline int polledSPI(u8* sendBuffer, u8* recvbuffer, u8 bytes);

static inline int intrSPI(u8* sendbuffer, u8* recvbuffer, u8 bytes);

int prepareBuffers(u8* send, u8* recv);

int main(void) {
	int status;
	u8* send = NULL;
	u8* recv = NULL;

	xil_printf("-- SPI_PS Test with interrupt system v1.0 --\n");

	led_inst = malloc(sizeof(led_inst));
	if (led_inst == NULL)
		goto exit_failure;

	spi_inst = malloc(sizeof(spi_inst));
	if (spi_inst == NULL)
		goto exit_failure;

	intc_inst = malloc(sizeof(intc_inst));
	if (intc_inst)
		goto exit_failure;

	status = setupGpio();
	if (status)
		goto exit_failure;

	status = setupSpi(64, 0, 1);
	if (status)
		goto exit_failure;

	status = setupInterruptSystem();
	if (status)
		goto exit_failure;

	xil_printf("%s: System ready.\n", __func__);

	status = prepareBuffers(send, recv);
	if (status)
		goto exit_failure;

	while(1) {
		polledSPI(send, recv, MAX_BUF_SIZE);
		for(volatile int i = 0; i < 0xff; i++);
	}

	return XST_SUCCESS;

exit_failure:
	xil_printf("%s: Failed.\n", __func__);
	free(led_inst);
	free(spi_inst);
	free(intc_inst);
	return XST_FAILURE;
}

int setupInterruptSystem(void) {
	int status;
	XScuGic_Config* intc_config;

	intc_config = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (intc_config  == NULL)
		goto exit_failure;

	status = XScuGic_CfgInitialize(intc_inst, intc_config, intc_config->CpuBaseAddress);
	if (status)
		goto exit_failure;

	XScuGic_SetPriorityTriggerType(intc_inst, SPI_INTERRUPT_ID, SPI_PRIORITY, SPI_TRIGGER);

	status = XScuGic_Connect(intc_inst, SPI_INTERRUPT_ID, (Xil_ExceptionHandler)spiISR, spi_inst);
	if (status)
		goto exit_failure;

	status = XScuGic_SelfTest(intc_inst);
	if (status)
		goto exit_failure;

	XScuGic_Enable(intc_inst, SPI_INTERRUPT_ID);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, intc_inst);
	Xil_ExceptionEnable();

	xil_printf("%s: Successful.\n", __func__);
	return XST_SUCCESS;

exit_failure:
	xil_printf("%s: Failed.\n", __func__);
	free(intc_config);
	return XST_FAILURE;
}

int setupGpio(void) {
	int status;
	XGpio_Config* led_config;

	led_config = XGpio_LookupConfig(LED_DEVICE_ID);
	if (led_config == NULL)
		return XST_FAILURE;

	status = XGpio_CfgInitialize(led_inst, led_config, led_config->BaseAddress);
	if (status != XST_SUCCESS)
		goto exit_failure;

	// selbsttest der gpios
	XGpio_SelfTest(led_inst);

	// setze datenrichtung
	XGpio_SetDataDirection(led_inst, 1,	~LED_MASK);

	xil_printf("%s: Successful.\n", __func__);

	return XST_SUCCESS;

exit_failure:
	xil_printf("%s: Failed.\n", __func__);
	free(led_config);
	return XST_FAILURE;
}

int setupSpi(u32 prescale, u32 cpha, u32 cpol) {
	int status;
	XSpiPs_Config* spi_config;
	spi_config = XSpiPs_LookupConfig(SPI_DEVICE_ID);
	if (spi_config == NULL)
		goto exit_failure;

	status = XSpiPs_CfgInitialize(spi_inst, spi_config, spi_config->BaseAddress);
	if (status)
		goto exit_failure;

	status = XSpiPs_SelfTest(spi_inst);
	if (status)
		goto exit_failure;

	status = XSpiPs_SetOptions(spi_inst, XSPIPS_MASTER_OPTION |  \
			XSPIPS_FORCE_SSELECT_OPTION | \
			(cpha ? XSPIPS_CLK_PHASE_1_OPTION : 0) |  \
			(cpol ? XSPIPS_CLK_ACTIVE_LOW_OPTION : 0));
	if (status)
		goto exit_failure;

	status = XSpiPs_SetClkPrescaler(spi_inst, XSPIPS_CLK_PRESCALE_64);
	if (status)
		goto exit_failure;

	xil_printf("%s: Successful.\n", __func__);

	return XST_SUCCESS;

exit_failure:
	xil_printf("%s: Failed.\n", __func__);
	free(spi_config);
	return XST_FAILURE;
}

void spiISR(void* instancePtr) {
	// TODO
}

static inline int polledSPI(u8* sendBuffer, u8* recvBuffer, u8 bytes) {
	return XSpiPs_PolledTransfer(spi_inst, sendBuffer, recvBuffer, bytes);
}

static inline int intrSPI(u8* sendbuffer, u8* recvbuffer, u8 bytes) {
	// TODO
	return XST_SUCCESS;
}

int prepareBuffers(u8* send, u8* recv) {
	send = malloc(MAX_BUF_SIZE*sizeof(u8));
	if (send == NULL) {
		xil_printf("%s: Malloc buffer failed.\n", __func__);
		goto exit_failure;
	}

	recv = malloc(MAX_BUF_SIZE*sizeof(u8));
	if (recv == NULL) {
		xil_printf("%s: Malloc buffer failed.\n", __func__);
		goto exit_failure;
	}
	memset(send, 0, (size_t) MAX_BUF_SIZE);
	memset(recv, 0, (size_t) MAX_BUF_SIZE);

	xil_printf("%s: Success.\n", __func__);
	return XST_SUCCESS;

exit_failure:
	xil_printf("%s: Malloc failed.\n", __func__);
	free(send);
	free(recv);
	return XST_FAILURE;
}
