#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

void print_usage(char *argv[]) {
  printf("usage: %s <ip of the host>\n", argv[0]);
  printf("\t -h - (required) ip of host\n");
  printf("\t -p - (required) port\n");
  printf("\t -a - create new database file\n");
}

int send_hello(int fd) {
  char buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_HELLO_REQ;
  hdr->len = 1;

  // send the hello request with the version we speak after the above basic
  // point to spot where we want to write the hello headers (proto)
  dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
  hello->proto = PROTO_VER;

  // convert to network endianness
  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);
  hello->proto = htons(hello->proto);

  // Send to server
  printf("Sending hello to server proto ver: %d\n", PROTO_VER);
  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req));
  printf("Done sending\n");

  // recv the response
  read(fd, buf, sizeof(buf));
  printf("Got res\n");

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("Protocol mismatch.\n");
    close(fd);
    return STATUS_ERROR;
  }

  printf("Server connected, protocol v%d.\n", PROTO_VER);

  return STATUS_SUCCESS;
}

int send_employee(int fd, char *addstr) {
  char buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_EMPLOYEE_ADD_REQ;
  hdr->len = 1;

  dbproto_employee_add_req *add_emp_body = (dbproto_employee_add_req *)&hdr[1];
  strncpy(&add_emp_body->data, addstr, sizeof(add_emp_body->data));

  // convert to network endianness
  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);

  // Send to server
  printf("Sending new employee to server addstr: %s\n", addstr);

  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req));
  printf("Done sending\n");

  // recv the response
  read(fd, buf, sizeof(buf));
  printf("Got res\n");

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("Improper addr message format: %s\n", addstr);
    close(fd);
    return STATUS_ERROR;
  }

  printf("Done creating new employee\n");

  return STATUS_SUCCESS;
}

int list_employee(int fd) {
  char buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_EMPLOYEE_LIST_REQ;
  hdr->len = 0;

  // convert to network endianness
  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);

  // Send to server
  printf("Sending list employees request\n");

  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req));
  printf("Done sending\n");

  // recv the response
  read(fd, buf, sizeof(buf));
  printf("Got res\n");

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("[MSG_ERROR]: Failed to list employees\n");
    close(fd);
    return STATUS_ERROR;
  }

  if (hdr->type == MSG_EMPLOYEE_LIST_RESP) {
    printf("Listing employees...\n");
    dbproto_employee_list_resp *emp_resp = (dbproto_hello_req *)&hdr[1];

    for (int i = 0; i < hdr->len; i++) {
      read(fd, emp_resp, sizeof(dbproto_employee_list_resp));
      emp_resp->hours = ntohl(emp_resp->hours);
      printf("%s, %s, %d\n", emp_resp->name, emp_resp->address,
             emp_resp->hours);
    }
  }

  printf("Done listing employee\n");

  return STATUS_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("wtf: %d\n", argc);
    print_usage(argv);
    return 0;
  }

  int c;
  char *addArg = NULL;
  char *portArg = NULL, *hostArg = NULL;
  unsigned short port = 0;
  bool list = false;

  while ((c = getopt(argc, argv, "p:h:a:l")) != -1) {
    switch (c) {
    case 'p':
      portArg = optarg;
      port = atoi(portArg);
      break;
    case 'h':
      hostArg = optarg;
      break;
    case 'a':
      addArg = optarg;
      break;
    case 'l':
      list = true;
      break;
    case '?':
      printf("Unknown option -%c\n", c);
      break;

    default:
      exit(EXIT_FAILURE);
    }
  }

  if (port == 0) {
    printf("Bad port: %s\n", portArg);
    exit(EXIT_FAILURE);
  }

  if (hostArg == NULL) {
    printf("Must specify host with -h\n");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in serverInfo = {0};

  serverInfo.sin_family = AF_INET;
  serverInfo.sin_port = inet_addr(hostArg);
  serverInfo.sin_port = htons(port);

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  int optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  if (connect(fd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) == -1) {
    perror("connect");
    close(fd);
    exit(EXIT_FAILURE);
  }
  printf("Connected to %s:%d\n", hostArg, port);

  int helloRes = send_hello(fd);
  if (helloRes != STATUS_SUCCESS) {
    close(fd);
    exit(EXIT_FAILURE);
  }

  if (addArg) {
    if (send_employee(fd, addArg) == STATUS_ERROR) {
      close(fd);
      exit(EXIT_FAILURE);
    }
  }

  if (list) {
    if (list_employee(fd) == STATUS_ERROR) {
      close(fd);
      exit(EXIT_FAILURE);
    }
  }

  close(fd);

  exit(EXIT_SUCCESS);
  printf("Exit then print?");
  return 0;
}
