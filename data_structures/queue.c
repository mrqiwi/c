#include <stdio.h>
#include <stdlib.h>

typedef struct queue_node {
    char data;
    struct queue_node *next_ptr;
} queue_t;

char dequeue(queue_t **head_ptr, queue_t **tail_ptr);
void enqueue(queue_t **head_ptr, queue_t **tail_ptr, char value);
int is_empty(queue_t *head_ptr);
void print_queue(queue_t *current_ptr);

int main(void)
{
    char item;
    queue_t *head_ptr = NULL;
    queue_t *tail_ptr = NULL;

    enqueue(&head_ptr, &tail_ptr, 'c');
    enqueue(&head_ptr, &tail_ptr, 'b');
    enqueue(&head_ptr, &tail_ptr, 'a');

    print_queue(head_ptr);

    item = dequeue(&head_ptr, &tail_ptr);
    printf("%c has been dequeued.\n", item);

    print_queue(head_ptr);

    return 0;
}

void enqueue(queue_t **head_ptr, queue_t **tail_ptr, char value)
{
    queue_t *new_ptr;

    new_ptr = malloc(sizeof(queue_t));

    if (new_ptr == NULL) {
        printf("%c not inserted. No memory available.\n", value);
        return;
    }

    new_ptr->data = value;
    new_ptr->next_ptr = NULL;

    if (is_empty(*head_ptr))
        *head_ptr = new_ptr;
    else
        (*tail_ptr)->next_ptr = new_ptr;

    *tail_ptr = new_ptr;
}

char dequeue(queue_t **head_ptr, queue_t **tail_ptr)
{
    char value;
    queue_t *temp_ptr;

    value = (*head_ptr)->data;
    temp_ptr = *head_ptr;
    *head_ptr = (*head_ptr)->next_ptr;

    if (*head_ptr == NULL)
        *tail_ptr = NULL;

    free(temp_ptr);

    return value;

}

int is_empty(queue_t *head_ptr)
{
    return head_ptr == NULL;
}

void print_queue(queue_t *current_ptr)
{
    if (current_ptr == NULL) {
        printf("Queue is empty.\n\n");
    } else {
        printf("The queue is:\n");

        while (current_ptr != NULL) {
            printf("%c --> ", current_ptr->data);
            current_ptr = current_ptr->next_ptr;
        }

        printf("NULL\n\n");
    }
}
