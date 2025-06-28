/**
 * Poll-type server that echoes the request sent by a user
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "srvpoll.h"

clientstate_t clientStates[MAX_CLIENTS];
void print_usage(char *argv[]) {
  printf("Usage: %s -n -f <database files>\n", argv[0]);
  printf("\t -n - create new database file\n");
  printf("\t -f - (required) path to database file\n");
  printf("\t -p - (required) path to database file\n");
}

void poll_loop(unsigned short port, struct dbheader_t *dbhdr,
               struct employee_t *employees) {
  // Socket code
  int new_conn_socket, /* listening for clients fd */
      conn_fd /* when we accept a connection we will temporarily save it to this
               */
      ,
      num_of_file_descriptors = 1, /* max number of file descriptors we have */
      free_slot_index /* temporary free slot fd id */;

  fd_set /* libc struct which represents a set of file descriptors. This
            watches the fd_set, which references(?) another set of file
            descriptors and can check which ones are ready for i/o */
      read_fds,
      write_fds;

  struct sockaddr_in server_info = {0}, client_addr = {0};
  socklen_t server_addr_len = sizeof(server_info);
  socklen_t client_addr_len = sizeof(client_addr);

  // Poll setup
  struct pollfd poll_fds[MAX_CLIENTS - 1];

  init_clients(clientStates);

  // Create listening socket
  if ((new_conn_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  };

  // Set SO_REUSEADDR option - makes socket non-waiting. Do this before binding
  // the socket!
  int optval = 1;
  if (setsockopt(new_conn_socket, SOL_SOCKET, SO_REUSEADDR, &optval,
                 sizeof(optval)) < 0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  // Set up server address structure
  // Clear server info to 0 bits
  memset(&server_info, 0, sizeof(server_info));
  server_info.sin_family = AF_INET;
  server_info.sin_addr.s_addr = INADDR_ANY;
  server_info.sin_port = htons(port);

  if (bind(new_conn_socket, (struct sockaddr *)&server_info,
           sizeof(server_info)) == -1) {
    perror("bind");
    close(new_conn_socket);
    exit(EXIT_FAILURE);
  }

  // Listen
  if (listen(new_conn_socket, BACKLOG) == -1) {
    perror("listen");
    close(new_conn_socket);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", port);

  // Initialize the poll_fds
  memset(poll_fds, 0, sizeof(poll_fds));
  // Special slot in index 0 for new connections socket
  poll_fds[0].fd = new_conn_socket;
  // Event is an input event
  poll_fds[0].events = POLLIN;
  // Currently we have 1 fd to check
  num_of_file_descriptors = 1;

  // Main loop

  while (1) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clientStates[i].fd != -1) {
        poll_fds[i + 1].fd =
            clientStates[i].fd; // Offset by 1 for new_conn_socket which will
                                // always be in pos 0
        poll_fds[i + 1].events = POLLIN;
      }
    }

    // Wait for an event on one of the sockets
    int n_events =
        poll(poll_fds, num_of_file_descriptors, -1); // -1 means no timeout
    if (n_events == -1) {
      perror("poll");
      exit(EXIT_FAILURE);
    }

    // Check for new connection
    if (poll_fds[0].revents & POLLIN) {
      if ((conn_fd = accept(new_conn_socket, (struct sockaddr *)&client_addr,
                            &client_addr_len)) == -1) {
        perror("accept");
        continue;
      }

      printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));

      free_slot_index = find_free_slot_index(clientStates);

      if (free_slot_index == -1) {
        printf("Server full: closing new connection\n");
        close(conn_fd);
      } else {
        clientStates[free_slot_index].fd = conn_fd;
        clientStates[free_slot_index].state = STATE_CONNECTED;
        num_of_file_descriptors++;
        printf("Slot %d has fd %d\n", free_slot_index,
               clientStates[free_slot_index].fd);
      }

      n_events--;
    }

    // Check each client for read/write activity
    for (/* Start from 1 to skip the new_conn_socket fd check */ int i = 1;
         i <= num_of_file_descriptors &&
         /* stop when we have no more events to process */ n_events > 0;
         i++) {
      if (poll_fds[i].revents & POLLIN) {
        int fd_in_poll_fds = poll_fds[i].fd;
        int index_in_client_states =
            find_slot_by_fd(clientStates, fd_in_poll_fds);

        printf("Reading %d, want fd: %d, slot index: %d, client buf: %s\n", i,
               fd_in_poll_fds, index_in_client_states,
               clientStates[index_in_client_states].buffer);
        ssize_t bytes_read =
            read(clientStates[index_in_client_states].fd,
                 &clientStates[index_in_client_states].buffer,
                 sizeof(clientStates[index_in_client_states].buffer));

        if (bytes_read <= 0) {
          // tcp level closing connection
          close(fd_in_poll_fds);
          if (index_in_client_states == -1) {
            printf("Tried to close fd that doesn't exist??\n");
          } else {
            clientStates[index_in_client_states].fd = -1; // Free up the slot
            // Clear client state buffer on disconnect
            memset(&clientStates[i].buffer, '\0',
                   sizeof(clientStates[i].buffer));
            clientStates[index_in_client_states].state = STATE_DISCONNECTED;
            printf("Client disconnected or error\n");
            num_of_file_descriptors--;
          }
        } else {
          printf("Received data from client: %s\n",
                 clientStates[index_in_client_states].buffer);
          handle_client_fsm(dbhdr, employees, clientStates);
        }
        n_events--;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int c;
  char *filepath = NULL;
  bool newfile = false;
  bool list = false;
  char *removeName = NULL;
  char *addstring = NULL;
  char *emp_to_update_name = NULL;
  int newHours = -1;
  int dbfd = -1;
  struct dbheader_t *dbhdr = NULL;
  struct employee_t *employees = NULL;
  int srvPort = -1;

  while ((c = getopt(argc, argv, "nlf:a:r:u:e:p:")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case 'p':
      srvPort = atoi(optarg);
      break;
    case '?':
      printf("Unknown option -%c\n", c);
      break;
    default:
      printf("Default main\n");
      exit(EXIT_FAILURE);
    }
  }

  if (filepath == NULL) {
    printf("Filepath is a required argument\n");
    print_usage(argv);

    return 0;
  }

  if (newfile) {
    dbfd = create_db_file(filepath);

    if (dbfd == STATUS_ERROR) {
      printf("Unable to create database file \"%s\"\n", filepath);
      exit(EXIT_FAILURE);
    }

    if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to create database header\n");
      exit(EXIT_FAILURE);
    }
  } else {
    dbfd = open_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      printf("Unable to open database file \"%s\"\n", filepath);
    }

    if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to validate database header\n");
      exit(EXIT_FAILURE);
    }
  }

  if (read_employees(dbfd, dbhdr, &employees) != STATUS_SUCCESS) {
    printf("Failed to read employees");
    return 0;
  };

  if (addstring) {
    add_employee(dbhdr, &employees, addstring);

    printf("Successfully created employee. New Employee count: %d\n",
           dbhdr->count);
  }

  if (list) {
    list_employees(dbhdr, employees);
  }

  if (removeName) {
    printf("Attempting to remove: %s\n", removeName);
    if (remove_employee_by_name(dbhdr, &employees, removeName) ==
        STATUS_ERROR) {
      printf("Failed to delete employee %s\n", removeName);
      exit(EXIT_FAILURE);
    }

    printf("Successfully deleted employee %s. New Employee count: %d\n",
           removeName, dbhdr->count);
  }

  if (emp_to_update_name) {
    // Check new name
    if (newHours == -1) {
      printf("New hours must be a positive number: %d", newHours);
      exit(EXIT_FAILURE);
    }

    update_employee_hours(dbhdr, &employees, emp_to_update_name, newHours);
    printf("Updated %s hours to %u\n", emp_to_update_name, newHours);
  }

  printf("Newfile: %s\n", newfile ? "true" : "false");
  printf("Filepath: %s\n", filepath);

  /* debug_db_header(dbhdr); */
  output_file(dbfd, dbhdr, employees);

  poll_loop(srvPort, dbhdr, employees);
  free(dbhdr);
  dbhdr = NULL;
  free(employees);
  employees = NULL;

  return 0;
}
