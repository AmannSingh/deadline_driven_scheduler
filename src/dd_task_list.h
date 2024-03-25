
#ifndef DD_TASK_LIST_H
#define DD_TASK_LIST_H

/* Standard includes*/
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"

//
// typedef enum task_type task_type;

typedef enum task_type
{
    PERIODIC,
    APERIODIC
} task_type;

/* TODO: Extend to include additional info (list of interrupt times ect usefull for debugging Monitor Task). */
typedef struct dd_task
{
    TaskHandle_t t_handle;
    task_type type;
    uint32_t task_id;
    uint32_t release_time;
    uint32_t absolute_deadline;
    uint32_t completion_time;
} dd_task;

typedef struct dd_task_node
{
    dd_task task;
    struct dd_task_node *next_task;

} dd_task_node;

void insert_at_front(dd_task_node **head, dd_task new_task);
void insert_at_back(dd_task_node **head, dd_task new_task);
void delete_at_front(dd_task_node **head);
dd_task pop(dd_task_node **head);
void sort_EDF(dd_task_node **head);

#endif // DD_TASK_LIST_H
