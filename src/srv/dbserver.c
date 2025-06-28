/**
 * Poll-type server that echoes the request sent by a user
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 1024
#define PORT 9999
#define BACKLOG 10
#define BUFF_SIZE 4096

typedef enum { // Connection State enum
  STATE_NEW,
  STATE_CONNECTED,
  STATE_DISCONNECTED
} state_e;

typedef struct {
  int fd;
  state_e state;
  char buffer[4096]; // Each client gets their own buffer
} clientstate_t;

// TLV - type length value system
typedef struct {
  state_e type;       // type of packet
  unsigned short len; // length

} proto_hdr_t;

clientstate_t clientStates[MAX_CLIENTS];

void init_clients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clientStates[i].fd =
        -1; // Initialized to an invalid file descriptor (not used)
    clientStates[i].state = STATE_NEW;
    memset(
        &clientStates[i].buffer, '\0',
        BUFF_SIZE); // Clear the buffer when initialising to make sure its empty
  }
}

// Returns the index of clientStates that is empty/free
// -1 if none is found. All clients are used
int find_free_slot_index() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clientStates[i].fd == -1) {
      return i;
    }
  }

  return -1;
}

int find_slot_by_fd(int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clientStates[i].fd == fd) {
      return i;
    }
  }

  return -1;
}

int main() {
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

  init_clients();

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
  server_info.sin_port = htons(PORT);

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

  printf("Server listening on port %d\n", PORT);

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

      free_slot_index = find_free_slot_index();

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
        int index_in_client_states = find_slot_by_fd(fd_in_poll_fds);

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
        }
        n_events--;
      }
    }
  }

  return 0;
}
