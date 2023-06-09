/*
 * scheduler.h
 *
 *  Created on: Jun 9, 2023
 *      Author: USER
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdint.h>

typedef enum
{
	READY = 0x00,
	BLOCKED = 0xFF
} TASK_STATE_t;

typedef struct
{
	uint32_t *psp_value;
	uint32_t block_count;
	TASK_STATE_t state;
	void (*task_handler)();
} TASK_t;

typedef struct
{
	uint32_t current_task;
	uint32_t tasks_num;
	uint32_t sys_tick_count;
} TASKS_CONTROL_t;

#define SRAM_END 					0x2001FFF0
#define TASK_STACK_SIZE				1024
#define SCHEDULER_STACK_SIZE		1024
#define TASKS_MAX_NUM				8
#define SYS_TICK_EXCEPTION_HZ		1000

void scheduler_init(void (**task_handlers)(), uint32_t task_handlers_len);
//Add all tasks before initing the stack. It relies on tasks num
void add_task(void (*task_handler)());
void init_SysTick();
void init_stack();
void switch_to_psp();

uint32_t* set_dummy_stack_frame(uint32_t task_num, uint32_t PC);
void unblock_and_choose_task();
void block_task(uint32_t delay);
void schedule_task();

void log_scheduler_state();

#endif /* SCHEDULER_H_ */
