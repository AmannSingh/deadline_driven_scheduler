#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
#include "../FreeRTOS_Source/include/task.h"
#include "dd_task_list.h"

typedef enum task_type
{
    PERIODIC,
    APERIODIC
} task_type;

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

void insertAtFront()
{
    dd_task task;
    struct dd_task_node *temp;
    temp = malloc(sizeof(struct node));
    printf("\nEnter number to"
           " be inserted : ");
    scanf("%d", &data);
    temp->info = data;

    // Pointer of temp will be
    // assigned to start
    temp->link = start;
    start = temp;
}
