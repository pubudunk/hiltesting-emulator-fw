/*
 * test_case_handler.h
 *
 *  Created on: Aug 6, 2024
 *      Author: pubuduk
 */

#ifndef INC_TEST_CASE_HANDLER_H_
#define INC_TEST_CASE_HANDLER_H_

/* test case handlers */
void vGPIO_O_Test_Handler( void * pvParameters );
void vGPIO_IO_Test_Handler( void * pvParameters );
void vUART_Test_Handler( void * pvParameters );
void vI2C_Test_Handler( void * pvParameters );
void vCAN_Test_Handler( void * pvParameters );

#endif /* INC_TEST_CASE_HANDLER_H_ */
