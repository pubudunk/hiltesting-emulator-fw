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

/* Defines */
#define CMB_BUFFER_SIZE		32
#define REP_BUFFER_SIZE		(CMB_BUFFER_SIZE)/* This is ok for now */


/* Command Protocol defines */

/* STX (1) | DATA_LEN (1) | PKT_TYPE (1) | NUM_TLVS (1) | TLVS (Var) | CRC (1) */
/* DATA_LEN - Length from PKT_TYPE field to CRC field both inclusive */

#define	STX					0x02

#define PKT_TYPE_TESTS		0x01
#define PKT_TYPE_RESULTS 	0x02

#define RESULT_PASS			0x01
#define RESULT_FAIL			0x02
#define RESULT_NA			0x03

#define POS_STX				0
#define POS_DATA_LEN		1
#define POS_PKT_TYPE		2
#define POS_DATA			(POS_PKT_TYPE)
#define POS_NUM_TLVS		3
#define POS_TLV_START		4

#define MIN_CMD_DATA_LEN	5 	/* TYPE + NUM_TLV + TLV0_ID + TLV0_LEN + CRC */


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
		printf("%02X ", pucBuffer[i]);
	}
	printf("\n");
}

void vComm_Uart_Handler( void * pvParameters )
{
	uint8_t pucCmdBuffer[CMB_BUFFER_SIZE];
	uint8_t pucReplyBuffer[REP_BUFFER_SIZE];
	uint8_t ucRet = HAL_OK;
	uint8_t ucDataLen = 0;
	uint8_t ucCrcCalc = 0, ucCrcRecv = 0;


	for(;;) {

//	  char *welcome_msg = "Welcome to HIL Testing Hardware Emulator\r\n";
//	  HAL_UART_Transmit(&huart2, (uint8_t *)welcome_msg, strlen(welcome_msg), HAL_MAX_DELAY);
//	  HAL_Delay(1000);

		memset(pucCmdBuffer, 0, sizeof(pucCmdBuffer));

		printf("Waiting for new command\n");

		/* Wait for STX - Infinite wait */
		HAL_UART_Receive(&huart2, &pucCmdBuffer[POS_STX], 0x01, HAL_MAX_DELAY);
		if( pucCmdBuffer[POS_STX] != STX) {
			printf("Invalid STX. Discarding\n");
			continue;
		}

		/* Read data len field. Change timeout to 10 sec */
		ucRet = HAL_UART_Receive(&huart2, &pucCmdBuffer[POS_DATA_LEN], 0x01, 10000);
		if( ucRet != HAL_OK ) {
			printf("HAL Error: %d. Go to start\n", ucRet);
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
			printf("Invalid cmd data length: %d\n", ucDataLen);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* Check packet type */
		if( pucCmdBuffer[POS_PKT_TYPE] != PKT_TYPE_TESTS ) {
			printf("Invalid packet type: %d\n", pucCmdBuffer[POS_PKT_TYPE]);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* check CRC */
		ucCrcRecv = pucCmdBuffer[POS_DATA + ucDataLen - 1];
		ucCrcCalc = prvCalc_CRC(pucCmdBuffer, ucDataLen + 1); /* ucLen - RCC + STX + LEN */
		if( ucCrcCalc != ucCrcRecv ) { /* POS CRC of the cmd pkt */
			printf("Invalid CRC: CrcCalc:%02X  CrcRecv:%02X  \n", ucCrcCalc, ucCrcRecv);
			/* TODO: Reply Error Message to sender */
			continue;
		}

		/* Packet validated. Parse TLVs and Notify Test Tasks */

		prvPrint_Pkt(pucCmdBuffer, ucDataLen + 2);

		//TODO: TLV parsing and Test Task Notification

		//TODO: Wait for Test Tasks complete

		//TODO: Read Test Task results

		/* Construct the reply packet */

		memset(pucReplyBuffer, 0, sizeof(pucReplyBuffer));


		/* TODO: Fillng with dummy data for now */
			pucReplyBuffer[POS_STX] = STX;
			pucReplyBuffer[POS_DATA_LEN] = 0x12;
			uint8_t tlv_data[] = {0x02, 0x05, 0x01, 0x01, 0x01, 0x02, 0x01, 0x02, 0x03, 0x01, 0x03, 0x04, 0x01, 0x02, 0x05, 0x01, 0x01};
			memcpy(&pucReplyBuffer[POS_DATA], tlv_data, sizeof(tlv_data));
			ucCrcCalc = prvCalc_CRC(pucReplyBuffer, 0x13);
			pucReplyBuffer[POS_DATA + 0x12 -1] = ucCrcCalc;

			prvPrint_Pkt(pucReplyBuffer, 0x12 + 2);

			HAL_UART_Transmit(&huart2, pucReplyBuffer, 0x12+2, 15000);


	}
}
