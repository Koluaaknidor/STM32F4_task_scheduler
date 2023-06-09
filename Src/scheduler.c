/*
 * scheduler.c
 *
 *  Created on: Jun 9, 2023
 *      Author: USER
 */
#include "scheduler.h"

#include <stdint.h>
#include <stdio.h>
#include "led.h"

TASK_t tasks[TASKS_MAX_NUM];
TASKS_CONTROL_t tasks_control =
{
	.current_task = 1,			//Init the first task
	.tasks_num = 0,
	.sys_tick_count = 0
};

void scheduler_init(void (**task_handlers)(), uint32_t task_handlers_len)
{
	if (task_handlers_len > TASKS_MAX_NUM)
	{
		//TODO: Error logic
	}
	//Set MSP to start of scheduler stack
	uint32_t scheduler_stack_start = (SRAM_END) - (TASK_STACK_SIZE * TASKS_MAX_NUM);
	__asm volatile("MSR MSP, %0"::"r"(scheduler_stack_start));

	led_init_all();

	for (int i=0; i<task_handlers_len; i++)
		add_task(task_handlers[i]);

	init_stack();

	init_SysTick();
	//TODO: What if SysTick is generated before switching to psp
	switch_to_psp();

	//Start the first task
	tasks[tasks_control.current_task].task_handler();
}

void add_task(void (*task_handler)())
{
	TASK_t task =
	{
		.state = READY,
		.task_handler = task_handler
	};
	tasks[tasks_control.tasks_num] = task;
	tasks_control.tasks_num++;
}

void init_SysTick()
{
	//Set timer reload value to 16000 - 1
	uint32_t *SYST_RVR = (uint32_t*)0xE000E014;
	*SYST_RVR &= 0xFF000000;
	*SYST_RVR |= (16000000 / SYS_TICK_EXCEPTION_HZ - 1);

	uint32_t *SYST_CSR = (uint32_t*)0xE000E010;
	//Clock source - proc. clock, generate exception
	*SYST_CSR |= (0x3 << 1);
	//Enable clock
	*SYST_CSR |= 1;
}

void init_stack()
{
	//Set dummy frames and save psp values
	for (int i=0; i<tasks_control.tasks_num; i++)
	{
		tasks[i].psp_value = set_dummy_stack_frame(i, (uint32_t)tasks[i].task_handler);
	}
}

__attribute__((naked)) void switch_to_psp()
{
	//Switch to psp, init it to start of the init task start
	__asm volatile("MSR PSP, %0"::"r"(SRAM_END - (TASK_STACK_SIZE * tasks_control.current_task)));
	__asm volatile("MOV R0, #0x02");
	__asm volatile("MSR CONTROL, R0");
	__asm volatile("BX LR");
}

uint32_t* set_dummy_stack_frame(uint32_t task_num, uint32_t PC)
{
	//TODO: check that no need to set LR in dummy frame.
	uint32_t *stack_pointer = (uint32_t*)((SRAM_END) - (TASK_STACK_SIZE * task_num));

	//xPSR
	stack_pointer--;
	*stack_pointer = 0x01000000;
	//PC
	stack_pointer--;
	*stack_pointer = PC;

//	stack_pointer--;
//	*stack_pointer = 0xFFFFFFFD;

	//Rest 14 registers
	for(int i=0; i<14; i++)
	{
		stack_pointer--;
		*stack_pointer = 0;
	}
	return stack_pointer;
}

void SysTick_Handler()
{
	tasks_control.sys_tick_count++;
	schedule_task();
}

/*
 * Task scheduling happens here
 */
__attribute__((naked)) void PendSV_Handler()
{
	__asm volatile("PUSH {LR}");
	__asm volatile("BL log_scheduler_state");
	__asm volatile("POP {LR}");

	/*Save prev task*/
	//Get PSP of interrupted task
	__asm volatile("MRS R0, PSP");
	//Store the second frame
	__asm volatile("STMDB R0!, {R4-R11}");
	//Save the new value of PSP
	__asm volatile("MOV %0, R0":"=r"(tasks[tasks_control.current_task].psp_value));

	/*Load next task*/
	__asm volatile("PUSH {LR}");
	__asm volatile("BL unblock_and_choose_task");
	__asm volatile("POP {LR}");
	//Get PSP
	__asm volatile("MOV R0, %0"::"r"(tasks[tasks_control.current_task].psp_value));
	//Retrieve r4-r11
	__asm volatile("LDMIA R0!, {R4-R11}");
	//Set PSP and exit
	__asm volatile("MSR PSP, R0");
	//Return (LR is stored with the special value which tells details about returnal from handler)
	__asm volatile("BX LR");
}

void log_scheduler_state()
{
#if 0
	printf("*+*+*+*+*+*+*TASKS CONTROL*+*+*+*+*+*+*\n");
	printf("Current task:%lu\n", tasks_control.current_task);
	printf("SysTick count:%lu\n", tasks_control.sys_tick_count);
	for (int i=0; i<tasks_control.tasks_num; i++)
	{
		printf("*+*+*+*+*+*+*TASK %d*+*+*+*+*+*+*\n", i);
		printf("PSP value:%lX\n", (uint32_t)tasks[i].psp_value);
		printf("Block count:%lu\n", tasks[i].block_count);
		printf("State:%d\n", tasks[i].state);
	}
	printf("*+*+*+*+*+*+*+*+*+*+*+*+*+**+*+*+*+*+*+*\n\n");
#endif
}

void unblock_and_choose_task()
{
	//Unblock all expired blocks
	for (int i=1; i<tasks_control.tasks_num; i++)
	{
		if (tasks[i].state == BLOCKED)
		{
			if (tasks_control.sys_tick_count == tasks[i].block_count)
			{
				tasks[i].state = READY;
			}
		}
	}

	//Schedule nearest READY task, else schedule idle
	int current_task = tasks_control.current_task;
	TASK_STATE_t task_state_check = BLOCKED;
	for(int i=0; i<tasks_control.tasks_num - 1; i++)//Iterate tasks_num - 1 times
	{
		current_task++;
		if (current_task == tasks_control.tasks_num)
			current_task = 1;
		if (tasks[current_task].state == READY)
		{
			task_state_check = READY;
			break;
		}
	}
	if (task_state_check == READY)
		tasks_control.current_task = current_task;
	else
		tasks_control.current_task = 0;
}

void block_task(uint32_t delay)
{
	//TODO: disable interrupts for the time of accessing shared global data
	tasks[tasks_control.current_task].block_count = tasks_control.sys_tick_count + delay;
	tasks[tasks_control.current_task].state = BLOCKED;

	schedule_task();
}

void schedule_task()
{
	//PendSV trigger
	uint32_t *ICSR = (uint32_t*)0xE000ED04;
	*ICSR |= (1 << 28);
}

void HardFault_Handler()
{
	printf("HardFault occurred\n");
	while(1)
	{
		led_on(LED_ORANGE);
		for(int i=0; i<100000; i++);
		led_off(LED_ORANGE);
		for(int i=0; i<100000; i++);
	}
}


