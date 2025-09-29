#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITY 5

static char *my_strdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *p = (char *)malloc(len);
  if (!p) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memcpy(p, s, len);
  return p;
}

static bool is_print_cmd(const char *s) {
  if (!s)
    return false;
  size_t len = strlen(s);
  if (len > 0 && s[len - 1] == '\n') {
    len--;
  }
  return (len == 5 && strncmp(s, "print", 5) == 0);
}

static void hist_add(char *hist[], int cap, int *count, int *next,
                     const char *line) {
  if (hist[*next] != NULL) {
    free(hist[*next]);
    hist[*next] = NULL;
  }
  hist[*next] = my_strdup(line);

  *next = (*next + 1) % cap;
  if (*count < cap) {
    (*count)++;
  }
}

static void hist_print(char *hist[], int cap, int count, int next) {
  if (count == 0) {
    return;
  }

  int start = (count == cap) ? next : 0;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % cap;
    if (hist[idx]) {
      fputs(hist[idx], stdout);
    }
  }
}

static void hist_free(char *hist[], int cap) {
  for (int i = 0; i < cap; i++) {
    free(hist[i]);
    hist[i] = NULL;
  }
}

int main(void) {
  char *history[CAPACITY] = {NULL};
  int count = 0;
  int next = 0;

  char *line = NULL;
  size_t bufcap = 0;

  for (;;) {
    printf("Enter input: ");
    fflush(stdout);

    ssize_t nread = getline(&line, &bufcap, stdin);
    if (nread == -1) {
      break;
    }

    hist_add(history, CAPACITY, &count, &next, line);

    if (is_print_cmd(line)) {
      hist_print(history, CAPACITY, count, next);
    }
  }

  free(line);
  hist_free(history, CAPACITY);
  return 0;
}
