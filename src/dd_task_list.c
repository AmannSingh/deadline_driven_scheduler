#include <dd_task_list.h>

void insert_at_front(dd_task_node **head, dd_task new_task)
{
    dd_task_node *new_node = (dd_task_node *)malloc(sizeof(dd_task_node));
    if (new_node == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    new_node->task = new_task;
    new_node->next_task = *head;
    *head = new_node;
}

void insert_at_back(dd_task_node **head, dd_task new_task)
{
    dd_task_node *new_node = (dd_task_node *)malloc(sizeof(dd_task_node));
    dd_task_node *last = *head;

    new_node->task = new_task;
    new_node->next_task = NULL;

    if (*head == NULL)
    {
        *head = new_node;
        return;
    }

    while (last->next_task != NULL)
    {
        last = last->next_task;
    }
    last->next_task = new_node;
}

// return entire node instead of task 

dd_task pop(dd_task_node **head)
{
    dd_task task;
    if (*head == NULL)
    {
        printf("List is empty.\n");
        return;
    }
    dd_task_node *temp = *head;
    task = temp->task;
    *head = (*head)->next_task;
    free(temp);
    return task;
}
/* Sort by absolute deadline, using bubble sort. */
void sort_EDF(dd_task_node **head)
{
    int is_swapped;
    dd_task_node *current;
    dd_task_node *last_sorted = NULL;

    if (*head == NULL)
        return;

    do
    {
        is_swapped = 0;
        current = *head;

        while (current->next_task != last_sorted)
        {
            if (current->task.absolute_deadline > current->next_task->task.absolute_deadline)
            {
                // Swap tasks
                dd_task temp_task = current->task;
                current->task = current->next_task->task;
                current->next_task->task = temp_task;
                is_swapped = 1;
            }
            current = current->next_task;
        }
        last_sorted = current;
    } while (is_swapped);
}

int get_list_count(dd_task_node *head)
{
    int count = 0;
    dd_task_node *current = head;

    while (current != NULL)
    {
        count++;
        current = current->next_task; // Move to the next node
    }
    return count;
}