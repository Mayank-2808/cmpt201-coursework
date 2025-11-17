/*
Questions to answer at top of client.c:
(You should not need to change the code in client.c)
1. What is the address of the server it is trying to connect to (IP address and
port number).
2. Is it UDP or TCP? How do you know?
3. The client is going to send some data to the server. Where does it get this
data from? How can you tell in the code?
4. How does the client program end? How can you tell that in the code?

Answer(i) The client connects to server address 127.0.0.1 (localhost) on port
8000. This comes from the ADDR macro ("127.0.0.1") and PORT macro (8000) that
are copied into addr.sin_addr and addr.sin_port.

Answer(ii) It uses TCP. The socket() call uses SOCK_STREAM as the type, which
corresponds to TCP sockets (AF_INET + SOCK_STREAM).

Answer(iii) The client reads data from standard input (stdin), i.e., whatever I
type in the terminal. This is because it calls read(STDIN_FILENO, ...) and then
writes that buffer to the socket.

Answer(iv) The client keeps reading from stdin and sending to the server while
read() returns more than 1 byte. When I send only a newline or hit EOF (so
read() returns ≤ 1), the while loop stops, then the code closes the socket
(close(sfd)) and exits with exit(EXIT_SUCCESS). That’s how the program ends.
*/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8000
#define BUF_SIZE 64
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main() {
  struct sockaddr_in addr;
  int sfd;
  ssize_t num_read;
  char buf[BUF_SIZE];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  int res = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
  if (res == -1) {
    handle_error("connect");
  }

  while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)) > 1) {
    if (write(sfd, buf, num_read) != num_read) {
      handle_error("write");
    }
    printf("Just sent %zd bytes.\n", num_read);
  }

  if (num_read == -1) {
    handle_error("read");
  }

  close(sfd);
  exit(EXIT_SUCCESS);
}

// server.c:

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 64
#define PORT 8000
#define LISTEN_BACKLOG 32

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

// Shared counters for: total # messages, and counter of clients (used for
// assigning client IDs)
int total_message_count = 0;
int client_id_counter = 1;

// Mutexs to protect above global state.
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_id_mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_info {
  int cfd;
  int client_id;
};

void *handle_client(void *arg) {
  struct client_info *client = (struct client_info *)arg;

  int cfd = client->cfd;
  int client_id = client->client_id;

  char buf[BUF_SIZE];
  ssize_t num_read;

  while ((num_read = read(cfd, buf, BUF_SIZE - 1)) > 0) {
    buf[num_read] = '\0';

    pthread_mutex_lock(&count_mutex);
    int msg_num = +total_message_count;
    pthread_mutex_unlock(&count_mutex);

    printf("Msg # %3d; Client ID %d: %s", msg_num, client_id, buf);
    fflush(stdout);
  }

  if (num_read == -1) {
    perror("read");
  }

  if (close(cfd) == -1) {
    perror("close");
  }

  printf("Ending thread for client %d\n", client_id);
  fflush(stdout);

  free(client);

  return NULL;
}

int main() {
  struct sockaddr_in addr;
  int sfd;

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  for (;;) {
    struct sockaddr_in caddr;
    socklen_t caddr_len = sizeof(caddr);

    int cfd = accept(sfd, (struct sockaddr *)&caddr, &caddr_len);

    if (cfd == -1) {
      if (errno == EINTR)
        continue;

      handle_error("accept");
    }

    pthread_mutex_lock(&client_id_mutex);
    int client_id = client_id_counter++;
    pthread_mutex_unlock(&client_id_mutex);

    printf("New client created, ID %d on socket FD %d\n", client_id, cfd);
    fflush(stdout);

    struct client_info *info = malloc(sizeof(struct client_info));

    if (info == NULL) {
      perror("malloc");
      close(cfd);
      continue;
    }

    info->cfd = cfd;
    info->client_id = client_id;

    pthread_t tid;
    int s = pthread_create(&tid, NULL, handle_client, info);

    if (s != 0) {
      errno = s;
      perror("pthread_create");
      close(cfd);
      free(info);
      continue;
    }

    s = pthread_detach(tid);

    if (s != 0) {
      errno = s;
      perror("pthreas_detach");
    }
  }

  if (close(sfd) == -1) {
    handle_error("close");
  }

  return 0;
}
