/*
 * logging.c
 *
 *  Created on: Aug 7, 2024
 *      Author: pubuduk
 */

#include <stdio.h>
#include <stdarg.h>

#include "logging.h"

#include "FreeRTOS.h"
#include "semphr.h"

extern SemaphoreHandle_t xLogMutex;

void Add_To_Log(const char *fmt, ...)
{
	if( xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE ) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		xSemaphoreGive(xLogMutex);
	}
}
