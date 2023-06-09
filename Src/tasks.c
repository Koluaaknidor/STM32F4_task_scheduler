/*
 * tasks.c
 *
 *  Created on: Jun 9, 2023
 *      Author: USER
 */
#include "tasks.h"

#include "main.h"
#include "led.h"
#include "scheduler.h"

void idle_task()
{
	while(1)
	{

	}
}

void task0()
{
	while(1)
	{
		led_on(LED_GREEN);
		block_task(1000);
		led_off(LED_GREEN);
		block_task(1000);
	}
}

void task1()
{
	while(1)
	{
		led_on(LED_RED);
		block_task(200);
		led_off(LED_RED);
		block_task(200);
	}
}

void task2()
{
	while(1)
	{
		led_on(LED_BLUE);
		block_task(400);
		led_off(LED_BLUE);
		block_task(400);
	}
}
