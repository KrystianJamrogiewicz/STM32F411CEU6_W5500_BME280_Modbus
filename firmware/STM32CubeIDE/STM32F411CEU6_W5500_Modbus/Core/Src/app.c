#include "app.h"
#include <stdio.h>  // sprintf
#include <string.h> // strlen


extern UART_HandleTypeDef huart1; // Declaring global variables from other files


int test_variable = 0;

void App_Init(void)
{
    char msg[64];
    sprintf(msg, "Hello world \r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
}

void App_Loop(void)
{
	char msg[100];
	test_variable ++;
	sprintf(msg, "Actual value test_variable: %d \r\n", test_variable);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    if (test_variable >= 10) test_variable = 0;

	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

    HAL_Delay(1000);
}
