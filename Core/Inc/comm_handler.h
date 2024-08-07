/*
 * comm_handler.h
 *
 *  Created on: Aug 6, 2024
 *      Author: pubuduk
 */

#ifndef COMM_HANDLER_H_
#define COMM_HANDLER_H_

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

#define HEADER_LEN			2

#define MIN_CMD_DATA_LEN	5 	/* TYPE + NUM_TLV + TLV0_ID + TLV0_LEN + CRC */

void vComm_Uart_Handler( void * pvParameters );

#endif /* COMM_HANDLER_H_ */
