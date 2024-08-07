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

/* Extern variables */
extern QueueHandle_t xTestTaskQueues[MAX_TEST_TASKS];
extern TaskHandle_t xtaskHandles[MAX_EMULATOR_TASKS];

void vGPIO_O_Test_Handler( void * pvParameters )
{
	BaseType_t xRetured = pdFAIL;
	struct TestTaskTLV xtaskTLV;
	uint32_t ulTaskId = (uint32_t)pvParameters;

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);	/* Task IDs start from 1 */
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			//TODO Execute the test
			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_PASS;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
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

	for(;;) {

		memset(&xtaskTLV, 0, sizeof(xtaskTLV));
		xRetured = xQueueReceive(xTestTaskQueues[ulTaskId - 1], &xtaskTLV, portMAX_DELAY);
		if( xRetured == pdTRUE ) {
			Add_To_Log("queue message received. Task: %ld, TVL id: %d len: %d\n",
					ulTaskId, xtaskTLV.ucId, xtaskTLV.ucLen);

			//TODO Execute the test
			/* Update the result TLV */
			xtaskTLV.ucLen = 0x01;
			xtaskTLV.ucData[0] = RESULT_FAIL;

			/* Send the data back to uart comm handler */
			xQueueSend(xTestTaskQueues[ulTaskId - 1],
					(void* )&xtaskTLV,
					pdMS_TO_TICKS(5000));

			/* Notify the command handler */
			xTaskNotifyGiveIndexed(xtaskHandles[TASK_ID_COMM], 0);

			/* Wait until data is read */
			xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
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
			xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
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
			xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
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
			xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
		} else {
			Add_To_Log("Queue receive error for task: %ld\n", ulTaskId);
			continue;
		}
	}
}
