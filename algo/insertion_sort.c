#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 10

void insertion_sort(int array[], int length);

int main(void)
{
    int i, array[SIZE];

    srand(time(NULL));
    for (i = 0; i < SIZE; i++)
        array[i] = rand() % 90 + 10;

    puts("Unsorted array:");
    for (i = 0; i < SIZE; i++)
        printf("%d ", array[i]);

    insertion_sort(array, SIZE);
    puts("");

    puts("Sorted array:");
    for (i = 0; i < SIZE; i++)
        printf("%d ", array[i]);

    return 0;
}

void insertion_sort(int array[], int length)
{
    int i, index, value;

    for (i = 1; i < length; i++) {
        index = i;
        value = array[i];

        while (index > 0 && array[index - 1] > value) {
            array[index] = array[index - 1];
            --index;
        }

        array[index] = value;
    }
}
