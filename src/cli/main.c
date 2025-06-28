#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define PORT 9999

void print_usage(char *argv[]) {
  printf("usage: %s <ip of the host>\n", argv[0]);
}

void send_hello(int fd) {
  char buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_HELLO_REQ;
  hdr->len = 1;

  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);

  write(fd, buf, sizeof(dbproto_hdr_t));

  printf("Server connected, protocol v1.\n");
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    print_usage(argv);
    return 0;
  }

  struct sockaddr_in serverInfo = {0};

  serverInfo.sin_family = AF_INET;
  serverInfo.sin_port = inet_addr(argv[1]);
  serverInfo.sin_port = htons(PORT);

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  send_hello(fd);

  close(fd);

  return 0;
}
