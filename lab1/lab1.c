#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  char *line = NULL;
  size_t size = 0;

  while (1) {
    printf("Please enter some text: ");
    ssize_t chars = getline(&line, &size, stdin);

    if (chars == -1) {
      break;
    }

    if (line[chars - 1] == '\n') {
      line[chars - 1] = '\0';
    }
    printf("Tokens:\n");

    char *saveptr;
    char *token = strtok_r(line, " ", &saveptr);

    while (token != NULL) {
      printf(" %s\n", token);
      token = strtok_r(NULL, " ", &saveptr);
    }
  }

  free(line);
  return 0;
}
