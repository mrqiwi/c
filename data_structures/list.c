#include <stdio.h>
#include <stdlib.h>

typedef struct list_node {
    char data;
    struct list_node *next_ptr;
} list_t;

void insert(list_t **s_ptr, char value);
char delete(list_t **s_ptr, char value);
void print_list(list_t *s_ptr);

int main(void)
{
    list_t *start_ptr = NULL;

    print_list(start_ptr);

    insert(&start_ptr, 'a');
    insert(&start_ptr, 'A');
    insert(&start_ptr, 'b');
    insert(&start_ptr, 'B');

    printf("start_ptr = %c\n", start_ptr->data);
    print_list(start_ptr);

    delete(&start_ptr, 'A');
    delete(&start_ptr, 'b');

    print_list(start_ptr);

    return 0;
}

void insert(list_t **s_ptr, char value)
{
    list_t *new_ptr = NULL;
    list_t *previous_ptr = NULL;
    list_t *current_ptr = NULL;

    new_ptr = malloc(sizeof(list_t));

    if (new_ptr == NULL) {
        printf("%c not inserted. No memory available.\n", value);
        return;
    }

    new_ptr->data = value;
    new_ptr->next_ptr = NULL;

    previous_ptr = NULL;
    current_ptr = *s_ptr;

    // буквы в список вставляются по алфавиту
    while (current_ptr != NULL && value > current_ptr->data) {
        previous_ptr = current_ptr;
        current_ptr = current_ptr->next_ptr;
    }

    if (previous_ptr == NULL) {
        new_ptr->next_ptr = *s_ptr;
        *s_ptr = new_ptr;
    } else {
        previous_ptr->next_ptr = new_ptr;
        new_ptr->next_ptr = current_ptr;
    }

}

char delete(list_t **s_ptr, char value)
{
    list_t *previous_ptr = NULL;
    list_t *current_ptr = NULL;
    list_t *temp_ptr = NULL;

    if (value == (*s_ptr)->data) {
        temp_ptr = *s_ptr;
        *s_ptr = (*s_ptr)->next_ptr;
        free(temp_ptr);
        return value;
    } else {
        previous_ptr = *s_ptr;
        current_ptr = (*s_ptr)->next_ptr;

        while (current_ptr != NULL && current_ptr->data != value) {
            previous_ptr = current_ptr;
            current_ptr = current_ptr->next_ptr;
        }

        if (current_ptr != NULL) {
            temp_ptr = current_ptr;
            previous_ptr->next_ptr = current_ptr->next_ptr;
            free(temp_ptr);
            return value;
        }
    }

    return '\0';
}

void print_list(list_t *s_ptr)
{
    if (s_ptr == NULL) {
        printf("List is empty.\n\n");
    } else {
        printf("The list is:\n");

        while (s_ptr != NULL) {
            printf("%c --> ", s_ptr->data);
            s_ptr = s_ptr->next_ptr;
        }
        printf("NULL\n\n");
    }
}
