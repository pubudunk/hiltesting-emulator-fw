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
extern CAN_HandleTypeDef hcan1;

static uint8_t ucIsCaptureComplete = 0;
static uint32_t ulLastCapture = 0;

uint32_t GetTIM2ClockFreq(void) {
    RCC_ClkInitTypeDef clkconfig;
    uint32_t pFLatency;
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    uint32_t apb1Prescaler = clkconfig.APB1CLKDivider;
    uint32_t apb1Freq = HAL_RCC_GetPCLK1Freq();

    // Timers on APB1 will have their clocks multiplied by 2 if APB1 prescaler is not 1
    return (apb1Prescaler == RCC_HCLK_DIV1) ? apb1Freq : (apb1Freq * 2);
}

void tim2_ch1_ic_callback(TIM_HandleTypeDef *htim)
{
	uint32_t ulCurrentCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

	if ( ucIsCaptureComplete == 0 ) {
		if( ulLastCapture != 0 ) {
			printf("ulLastCapture: %ld, ulCurrentCapture: %ld\n",ulLastCapture, ulCurrentCapture);
			uint32_t diff = ulCurrentCapture - ulLastCapture;
			float tim2CntFrq = GetTIM2ClockFreq() / (htim->Init.Prescaler + 1);
			float userSigPeriod = diff * (1 / tim2CntFrq);
			uint32_t userFreq = (uint32_t)(1.0 / userSigPeriod);

			ucIsCaptureComplete = 1;

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
			ulLastCapture = 0;
			ulSigFreq = 0;
			__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Set the CCR1 to 0

			/* Start capture timer */
			HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

			/* Wait on index 1 until notified by capture isr */
			xRetured = xTaskNotifyWaitIndexed(1, ULONG_MAX, 0, &ulSigFreq, pdMS_TO_TICKS(3000));
			if( xRetured == pdTRUE ) {

				/* Check if the frequency is in the expected range */
				if( (ulSigFreq >= SIGNAL_FREQ*0.95) && (ulSigFreq <= SIGNAL_FREQ*1.05)) {	/* Actual frequency is 100Hz */
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

/* CAN ID: 0x65, Data: byte 0 - sensor ID 0x01, byte 1 sensor data */

void can1_rxfifo0_msg_pending_callback(CAN_HandleTypeDef *hcan)
{
	HAL_StatusTypeDef status = 0;
	CAN_RxHeaderTypeDef sRxHdr = {0};
	uint8_t ucRxBuf[2] = {0};
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	uint32_t ulResult = 0;

	status = HAL_CAN_GetRxMessage( hcan, CAN_RX_FIFO0, &sRxHdr, ucRxBuf );
	if( status != HAL_OK ) {
		printf("HAL_CAN_GetRxMessage Error %d\n", status);
	}

	/* inspect the data is correct */
	if ( ucRxBuf[0] == 0x01 ) {
		/* sensor ID field is correct. Iassume data field is correct */
		ulResult = 1;
	}

	/* Notify the waiting task */
	xTaskNotifyIndexedFromISR(xtaskHandles[TASK_ID_CAN_TEST],
			1,
			ulResult,
			eSetValueWithOverwrite,
			&xHigherPriorityTaskWoken);	/* Notify index 1 with value of signal freq */
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void vCAN_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;
	HAL_StatusTypeDef status = HAL_ERROR;
	uint32_t ulResult = 0;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_FAIL;

			/* Activate interrupt */
			status = HAL_CAN_ActivateNotification(&hcan1,
					CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF);
			if( status == HAL_OK ) {

				/* Wait on index 1 until notified by CAN RX isr */
				xRetured = xTaskNotifyWaitIndexed(1, ULONG_MAX, 0, &ulResult, pdMS_TO_TICKS(3000));
				if( xRetured == pdTRUE ) {

					if( ulResult == 1 ) {	/* Correct CAN message received */
						xtaskTLV.ucData[0] = RESULT_PASS;
						Add_To_Log("CAN receive task success\n");
					} else {
						Add_To_Log("CAN receive task failed\n");
					}
				} else {
					Add_To_Log("CAN receive task timeout\n");
				}


				/* Deactivate interrupt */
				status = HAL_CAN_DeactivateNotification(&hcan1,
						CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF);
				if ( status != HAL_OK ) {
					Add_To_Log("HAL_CAN_DeactivateNotification failed. Error: %lx\n", status);
				}
			} else {
				Add_To_Log("HAL_CAN_ActivateNotification failed. Error: %lx\n", status);
			}

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
