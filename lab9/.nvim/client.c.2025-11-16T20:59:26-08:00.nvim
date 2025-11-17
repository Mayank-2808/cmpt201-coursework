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
