#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_history(char *history[], int history_count)
{
    for (int i = 0; i < history_count; i++) {
        printf("%s", history[i]);
    }
}

void add_history(char *history[], int *history_count, char *line)
{
    if (*history_count < 5) {
        history[*history_count] = line;
        (*history_count)++;
    } else {
        free(history[0]);

        for (int i = 0; i < 4; i++) {
            history[i] = history[i + 1];
        }

        history[4] = line;
    }
}

void free_history(char *history[], int history_count)
{
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
}

int main(void)
{
    char *history[5] = {NULL};
    int history_count = 0;

    char *line = NULL;
    size_t capacity = 0;

    while (1) {
        printf("Enter input: ");

        if (getline(&line, &capacity, stdin) == -1) {
            break;
        }

        int should_print = strcmp(line, "print\n") == 0;

        add_history(history, &history_count, line);

        line = NULL;
        capacity = 0;

        if (should_print) {
            print_history(history, history_count);
        }
    }

    free(line);
    free_history(history, history_count);

    return 0;
}
