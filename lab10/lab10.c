// client.c:

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];
  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);
    // prepare message
    // this pads the desination with NULL
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  exit(EXIT_SUCCESS);
}

// server.c:

/*
Questions to answer at top of server.c:
(You should not need to change client.c)
Understanding the Client:
1. How is the client sending data to the server? What protocol?
   - The client creates a TCP socket (AF_INET, SOCK_STREAM) and connects to
     127.0.0.1:8001. It then uses write() to send data over this TCP connection.
     So the protocol is TCP over IPv4.

2. What data is the client sending to the server?
   - The client has an array of 5 strings: "Hello", "Apple", "Car", "Green",
     and "Dog". Once per second it copies each string into a fixed-size buffer
     of 1024 bytes and sends that buffer to the server using write(). So the
     server receives 5 fixed-size messages per client.

Understanding the Server:
1. Explain the argument that the `run_acceptor` thread is passed as an argument.
   - In main(), we create a struct acceptor_args variable (aargs) that stores:
     a run flag, a pointer to the shared list_handle, and a pointer to the
     list mutex. We pass &aargs to pthread_create() so that run_acceptor()
     can read and modify these shared values (e.g., check aargs->run in its
     loop, and share the same list + mutex with all client threads).

2. How are received messages stored?
   - Each time a client thread reads a message, it allocates a new list_node
     and a data buffer, copies the message into the buffer, and adds the node
     to the end of a singly-linked list. The list has a dummy head node, and a
     list_handle struct that keeps a pointer to the last node and a count of
     how many messages have been stored.

3. What does `main()` do with the received messages?
   - main() waits until the total number of messages in the list is at least
     MAX_CLIENTS * NUM_MSG_PER_CLIENT. Then it stops the acceptor thread,
     checks that enough messages were received, and calls collect_all() on
     the list. collect_all() prints each message ("Collected: ..."), frees the
     list nodes and data, and returns how many messages were collected.
     main() compares this count with list_handle.count and prints whether
     all messages were collected.

4. How are threads used in this sample code?
   - There is one acceptor thread that listens for incoming connections and
     creates a new client thread for each accepted socket (up to MAX_CLIENTS).
     Each client thread handles reading messages from its own client socket and
     inserting them into the shared list. main() runs in the original thread and
     coordinates everything: it starts the acceptor thread, waits for enough
     messages, then tells the acceptor to stop and joins it.

Use of non-blocking sockets in this lab:
- How are sockets made non-blocking?
  - The helper function set_non_blocking() uses fcntl(): it first gets the
    current flags with F_GETFL, then sets the O_NONBLOCK flag using F_SETFL.
    This is called on the listening socket in run_acceptor() and on each client
    socket in run_client().

- What sockets are made non-blocking?
  - The listening server socket (sfd) is made non-blocking in run_acceptor().
    Each accepted client socket (cfd) is also made non-blocking in run_client().

- Why are these sockets made non-blocking? What purpose does it serve?
  - With non-blocking sockets, accept() and read() return immediately instead
    of blocking the thread forever when there is no new connection or data.
    This lets the acceptor thread loop and keep checking the run flag, and it
    lets client threads keep running and eventually exit when run is set to
    false. In other words, non-blocking I/O makes it easier to stop threads and
    to handle multiple clients without any thread getting stuck waiting.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUF_SIZE 1024
#define PORT 8001
#define LISTEN_BACKLOG 32
#define MAX_CLIENTS 4
#define NUM_MSG_PER_CLIENT 5

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last;
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;

  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

struct acceptor_args {
  atomic_bool run;

  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

int init_server_socket() {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
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

  return sfd;
}

// Set a file descriptor to non-blocking mode
void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
}

void add_to_list(struct list_handle *list_handle, struct list_node *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

int collect_all(struct list_node head) {
  struct list_node *node = head.next; // get first node after head
  uint32_t total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    // Free node and advance to next item
    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return total;
}

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[BUF_SIZE];

  while (cargs->run) {
    ssize_t bytes_read = read(cfd, &msg_buf, BUF_SIZE);
    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket!\n");
        break;
      }
    } else if (bytes_read > 0) {
      // Create node with data
      struct list_node *new_node = malloc(sizeof(struct list_node));
      new_node->next = NULL;
      new_node->data = malloc(BUF_SIZE);
      memcpy(new_node->data, msg_buf, BUF_SIZE);

      struct list_handle *list_handle = cargs->list_handle;

      pthread_mutex_lock(cargs->list_lock);
      add_to_list(list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    }
  }

  if (close(cfd) == -1) {
    perror("client thread close");
  }
  return NULL;
}

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (aargs->run) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);
      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          handle_error("accept");
        }
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd = cfd;
        client_args[num_clients].run = true;
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock = aargs->list_lock;
        num_clients++;

        if (pthread_create(&threads[num_clients - 1], NULL, run_client,
                           &client_args[num_clients - 1]) != 0) {
          perror("pthread_create");
          close(cfd);
        }
      }
    }
  }

  printf("Not accepting any more clients!\n");

  // Shutdown and cleanup
  for (int i = 0; i < num_clients; i++) {
    client_args[i].run = false;

    if (pthread_join(threads[i], NULL) != 0)
      perror("pthread_join");
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }
  return NULL;
}

int main() {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  // List to store received messages
  // - Do not free list head (not dynamically allocated)
  struct list_node head = {NULL, NULL};
  struct list_node *last = &head;
  struct list_handle list_handle = {
      .last = &head,
      .count = 0,
  };

  pthread_t acceptor_thread;
  struct acceptor_args aargs = {
      .run = true,
      .list_handle = &list_handle,
      .list_lock = &list_mutex,
  };
  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);

  uint32_t needed = MAX_CLIENTS * NUM_MSG_PER_CLIENT;

  while (1) {
    pthread_mutex_lock(&list_mutex);
    uint32_t current = list_handle.count;
    pthread_mutex_unlock(&list_mutex);

    if (current >= needed)
      break;

    sleep(1);
  }

  aargs.run = false;
  pthread_join(acceptor_thread, NULL);

  if (list_handle.count != MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
    printf("Not enough messages were received!\n");
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);
  if (collected != list_handle.count) {
    printf("Not all messages were collected!\n");
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);

  return 0;
}
