/*
 * comm_handler.c
 *
 *  Created on: Aug 6, 2024
 *      Author: pubuduk
 *
 */

/* Includes */
#include "main.h"
#include "comm_handler.h"

#include <string.h>
#include <stdio.h>

/* Extern variable */
extern UART_HandleTypeDef huart2;
extern QueueHandle_t xTestTaskQueues[MAX_TEST_TASKS];
extern TaskHandle_t xtaskHandles[MAX_EMULATOR_TASKS];

/* Defines */
#define CMB_BUFFER_SIZE		32
#define REP_BUFFER_SIZE		(CMB_BUFFER_SIZE)/* This is ok for now */


static uint8_t prvCalc_CRC(uint8_t *pucBuffer, uint8_t ucLen)
{
	uint8_t ucCRC = 0;
	for(uint8_t i = 0; i < ucLen; i++) {
		ucCRC ^= pucBuffer[i];
	}

	return ucCRC;
}

static void prvPrint_Pkt(uint8_t *pucBuffer, uint8_t ucLen)
{
	for(uint8_t i = 0; i < ucLen; i++) {
		Add_To_Log("%02X ", pucBuffer[i]);
	}
	Add_To_Log("\n");
}

void vComm_Uart_Handler( void * pvParameters )
{
	uint8_t pucCmdBuffer[CMB_BUFFER_SIZE];
	uint8_t pucReplyBuffer[REP_BUFFER_SIZE];
	uint8_t ucRet = HAL_OK;
	uint8_t ucDataLen = 0;
	uint8_t ucCrcCalc = 0, ucCrcRecv = 0;
	uint8_t ucNumLlvs = 0;
	uint8_t ucTestId = 0, ucTestDataLen = 0;
	uint8_t index = 0;
	uint32_t ulTestTaskReceivedFlag = 0;	/* Used to set corresponding bit (test ID) if the test is received */
	uint8_t ucTastsToWait = 0;
	BaseType_t xReturned = pdFAIL;

	struct TestTaskTLV xTaskTLVs[MAX_TEST_TASKS];


	for(;;) {

//	  char *welcome_msg = "Welcome to HIL Testing Hardware Emulator\r\n";
//	  HAL_UART_Transmit(&huart2, (uint8_t *)welcome_msg, strlen(welcome_msg), HAL_MAX_DELAY);
//	  HAL_Delay(1000);

		memset(pucCmdBuffer, 0, sizeof(pucCmdBuffer));
		ulTestTaskReceivedFlag = 0;
		ucTastsToWait = 0;

		Add_To_Log("Waiting for new command\n");

		/* Wait for STX - Infinite wait */
		HAL_UART_Receive(&huart2, &pucCmdBuffer[POS_STX], 0x01, HAL_MAX_DELAY);
		if( pucCmdBuffer[POS_STX] != STX) {
			Add_To_Log("Invalid STX. Discarding\n");
			continue;
		}

		/* Read data len field. Change timeout to 10 sec */
		ucRet = HAL_UART_Receive(&huart2, &pucCmdBuffer[POS_DATA_LEN], 0x01, 10000);
		if( ucRet != HAL_OK ) {
			Add_To_Log("HAL Error: %d. Go to start\n", ucRet);
			continue;
		}

		/* Read data */
		/* Adjust the read data len for the buffer size. Two bytes are allocated for STX and LEN */
		ucDataLen = pucCmdBuffer[POS_DATA_LEN] <= (CMB_BUFFER_SIZE - 2)
				? pucCmdBuffer[POS_DATA_LEN] : (CMB_BUFFER_SIZE - 2);

		HAL_UART_Receive(&huart2, &pucCmdBuffer[POS_DATA], ucDataLen, 10000);

		/* Check min cmd pkt length */
		if( ucDataLen < MIN_CMD_DATA_LEN ) {
			/* invalid length. Readout remaining bytes if any and go to start */
			Add_To_Log("Invalid cmd data length: %d\n", ucDataLen);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* Check packet type */
		if( pucCmdBuffer[POS_PKT_TYPE] != PKT_TYPE_TESTS ) {
			Add_To_Log("Invalid packet type: %d\n", pucCmdBuffer[POS_PKT_TYPE]);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* check CRC */
		ucCrcRecv = pucCmdBuffer[POS_DATA + ucDataLen - 1];
		ucCrcCalc = prvCalc_CRC(pucCmdBuffer, ucDataLen + 1); /* ucLen - RCC + STX + LEN */
		if( ucCrcCalc != ucCrcRecv ) { /* POS CRC of the cmd pkt */
			Add_To_Log("Invalid CRC: CrcCalc:%02X  CrcRecv:%02X  \n", ucCrcCalc, ucCrcRecv);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* Packet validated. Parse TLVs and Notify Test Tasks */

		prvPrint_Pkt(pucCmdBuffer, HEADER_LEN + ucDataLen);

		//TODO: handle the case incorrect datalen is used. In this case access buffer may overflow */
		ucNumLlvs = pucCmdBuffer[POS_NUM_TLVS];
		index = POS_TLV_START;
		for( uint8_t i = 0; i < ucNumLlvs; i++ ) {

			ucTestId = pucCmdBuffer[index];
			ucTestDataLen = pucCmdBuffer[index + 1];

			/* Check if the test sent is valid */
			if(( ucTestId >= 0x01 ) && ( ucTestId <= 0x05 )) {
				xTaskTLVs[ucTestId - 1].ucId = ucTestId;	/* test task indices are zero based */
				xTaskTLVs[ucTestId - 1].ucLen = ucTestDataLen;
				memcpy(&xTaskTLVs[ucTestId - 1].ucData,
											&pucCmdBuffer[index + 2], ucTestDataLen);	//TODO: Properly test this case

				index += (2 + ucTestDataLen);

				xQueueSend(xTestTaskQueues[ucTestId - 1],
						(void* )&xTaskTLVs[ucTestId - 1],
						pdMS_TO_TICKS(5000));

				ulTestTaskReceivedFlag |= (0x01 << (ucTestId - 1));	/* Set the bit as test started */
				ucTastsToWait++;
			} else {
				//TODO: Handle the case when incorrect test ID is sent
			}
		}

		/* Wait for Test Tasks complete */
		uint8_t ucNotifications = 0;
		while (ucNotifications < ucTastsToWait) {
			ucNotifications += ulTaskNotifyTakeIndexed(0, pdTRUE, pdMS_TO_TICKS(10000));
		}


		/* Copy result data */
		memset(pucReplyBuffer, 0, sizeof(pucReplyBuffer));
		index = POS_TLV_START;
		ucNumLlvs = 0;
		ucDataLen = 2;	/* Start with packet type and num_tlv fields */
		for(uint8_t i = 0; i < MAX_TEST_TASKS; i++) {
			if( ulTestTaskReceivedFlag & (0x01 << i) ) {
				/* Test started. Should wait for response */
				xReturned = xQueueReceive(xTestTaskQueues[i], &xTaskTLVs[i], pdMS_TO_TICKS(10000));
				if( xReturned == pdPASS ) {
					pucReplyBuffer[index] = xTaskTLVs[i].ucId;
					pucReplyBuffer[index + 1] = xTaskTLVs[i].ucLen;
					memcpy(&pucReplyBuffer[index + 2], &xTaskTLVs[i].ucData,
							xTaskTLVs[i].ucLen);
					index += (2 + xTaskTLVs[i].ucLen);
					ucDataLen += (2 + xTaskTLVs[i].ucLen);
				} else {
					/* Reply timed out. Report RESULT_FAIL */
					pucReplyBuffer[index] = i + 1;
					pucReplyBuffer[index + 1] = 1;
					pucReplyBuffer[index + 2] = RESULT_FAIL;
					index += 3;
					ucDataLen += 3;
				}
				ucNumLlvs++;

				/* Single the task that data is read */
				xTaskNotify(xtaskHandles[i + 1], 0x00, eNoAction); /* Test task indices start from 1 */
			} else {
				Add_To_Log("Test id: %d is not received for waiting\n", i+1);
			}
		}

		/* Fill remainder of the reply packet */
		pucReplyBuffer[POS_STX] = STX;
		pucReplyBuffer[POS_DATA_LEN] = ++ucDataLen;	/* Extra byte for CRC */
		pucReplyBuffer[POS_PKT_TYPE] = PKT_TYPE_RESULTS;
		pucReplyBuffer[POS_NUM_TLVS] = ucNumLlvs;
		ucCrcCalc = prvCalc_CRC(pucReplyBuffer, HEADER_LEN + ucDataLen - 1);  /* ucDatalen contains CRC byte also so substract */
		pucReplyBuffer[POS_DATA + ucDataLen - 1] = ucCrcCalc;

		prvPrint_Pkt(pucReplyBuffer, HEADER_LEN + ucDataLen);

		/* Send reply */
		HAL_UART_Transmit(&huart2, pucReplyBuffer, HEADER_LEN + ucDataLen, 15000);

		//memset(pucReplyBuffer, 0, sizeof(pucReplyBuffer));


		/* TODO: Fillng with dummy data for now */
//			pucReplyBuffer[POS_STX] = STX;
//			pucReplyBuffer[POS_DATA_LEN] = 0x12;
//			uint8_t tlv_data[] = {0x02, 0x05, 0x01, 0x01, 0x01, 0x02, 0x01, 0x02, 0x03, 0x01, 0x03, 0x04, 0x01, 0x02, 0x05, 0x01, 0x01};
//			memcpy(&pucReplyBuffer[POS_DATA], tlv_data, sizeof(tlv_data));
//			ucCrcCalc = prvCalc_CRC(pucReplyBuffer, 0x13);
//			pucReplyBuffer[POS_DATA + 0x12 -1] = ucCrcCalc;
//
//			prvPrint_Pkt(pucReplyBuffer, 0x12 + 2);
//
//			HAL_UART_Transmit(&huart2, pucReplyBuffer, 0x12+2, 15000);


	}
}
