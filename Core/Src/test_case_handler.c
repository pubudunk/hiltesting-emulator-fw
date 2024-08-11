/*
 * test_case_handler.c
 *
 *  Created on: Aug 6, 2024
 *      Author: pubuduk
 */


/* Includes */
#include "main.h"
#include "test_case_handler.h"
#include "comm_handler.h" /* For protocol definitions */

#include <string.h>
#include <stdio.h>
#include <limits.h>

/* extern variables */
extern QueueHandle_t xTestTaskQueues[MAX_TEST_TASKS];
extern TaskHandle_t xtaskHandles[MAX_EMULATOR_TASKS];

extern TIM_HandleTypeDef htim2;

static uint8_t ucIsCaptureComplete = 0;

void tim2_ch1_ic_callback(TIM_HandleTypeDef *htim)
{
	static uint32_t ulLastCapture = 0;

	uint32_t ulCurrentCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

	if ( ucIsCaptureComplete == 0 ) {
		if( ulLastCapture != 0 ) {
			uint32_t diff = ulCurrentCapture - ulLastCapture;
			float tim2CntFrq = 2 * HAL_RCC_GetPCLK1Freq() / (htim->Init.Prescaler + 1);
			float userSigPeriod = diff * (1 / tim2CntFrq);
			uint32_t userFreq = 1 / userSigPeriod;

			ucIsCaptureComplete = 1;
			ulLastCapture = 0;

			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xTaskNotifyIndexedFromISR(xtaskHandles[TASK_ID_O_TEST],
					1,
					userFreq,
					eSetValueWithOverwrite,
					&xHigherPriorityTaskWoken);	/* Notify index 1 with value of signal freq */
			portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
		}
		ulLastCapture = ulCurrentCapture;
	}
}

#define SIGNAL_FREQ		100

void vGPIO_O_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;
	uint32_t ulSigFreq = 0;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);	/* Task IDs start from 1 */
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_FAIL;

			ucIsCaptureComplete = 0;	/* Reset flag */
			ulSigFreq = 0;
			__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Set the CCR1 to 0

			/* Start capture timer */
			HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

			/* Wait on index 1 until notified by capture isr */
			xRetured = xTaskNotifyWaitIndexed(1, ULONG_MAX, 0, &ulSigFreq, pdMS_TO_TICKS(3000));
			if( xRetured == pdTRUE ) {

				/* Check if the frequency is in the expected range */
				if( (ulSigFreq >= SIGNAL_FREQ*0.9) && (ulSigFreq <= SIGNAL_FREQ*1.1)) {	/* Actual frequency is 10Hz */
					xtaskTLV.ucData[0] = RESULT_PASS;
					Add_To_Log("Input Capture task success freq: %ld\n", ulSigFreq);
				} else {
					Add_To_Log("Input Capture task incorrect freq: %ld\n", ulSigFreq);
				}
			} else {
				Add_To_Log("Input Capture task timeout\n");
			}

			/* Stop capture timer */
			HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_1);

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}

void vGPIO_IO_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;
	uint8_t count = 0;
	uint8_t ucResult = RESULT_FAIL;

	for(;;) {

		count = 0;
		ucResult = RESULT_FAIL;
		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			/* Enable Input for DUT */
			HAL_GPIO_WritePin(BTN_OUT_GPIO_Port, BTN_OUT_Pin, GPIO_PIN_SET);

			vTaskDelay(pdMS_TO_TICKS(10));

			/* Check DUT Output - check for 1 second in total */
			while(count < 10) {
				if( HAL_GPIO_ReadPin(BTN_LED_IN_GPIO_Port, BTN_LED_IN_Pin)== GPIO_PIN_SET ) {
					ucResult = RESULT_PASS;
					break;
				} else {
					count++;
					vTaskDelay(pdMS_TO_TICKS(100));
				}
			}


			/* Disable Input for DUT */
			HAL_GPIO_WritePin(BTN_OUT_GPIO_Port, BTN_OUT_Pin, GPIO_PIN_RESET);


			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = ucResult;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}

void vUART_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			//TODO Execute the test
			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_NA;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}

void vI2C_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			//TODO Execute the test
			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_NA;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));


			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}

void vCAN_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			//TODO Execute the test
			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_NA;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}
