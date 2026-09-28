#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  char *prog = NULL;
  size_t size = 0;

  while (1) {
    printf("Enter programs to run.\n");
    printf("> ");
    fflush(stdout);

    ssize_t chars_read = getline(&prog, &size, stdin);

    if (chars_read == -1) {
      if (feof(stdin)) {
        break;
      }

      perror("getline");
      free(prog);
      return EXIT_FAILURE;
    }

    prog[strcspn(prog, "\n")] = '\0';

    pid_t pid = fork();

    if (pid == -1) {
      perror("fork");
      continue;
    }

    if (pid == 0) {
      execlp(prog, prog, (char *)NULL);

      perror("Exec failure");
      _exit(EXIT_FAILURE);
    }

    int status;

    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
    }
  }

  free(prog);

  return EXIT_SUCCESS;
}
