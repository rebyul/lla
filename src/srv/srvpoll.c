#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "common.h"
#include "srvpoll.h"

void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t *employees,
                       clientstate_t *client) {
  dbproto_hdr_t *hdr = (dbproto_hdr_t *)client->buffer;

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (client->state == STATE_HELLO) {
    if (hdr->type != MSG_HELLO_REQ || hdr->len != 1) {
      printf("Client state: HELLO but header type != HELLO_REQ or len !=1\n");
      // TODO: send err msg
    }

    // Move 1 header size in the client->buffer and cast to dbproto_hello_req
    dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
    hello->proto = ntohs(hello->proto);
    if (hello->proto != PROTO_VER) {
      printf("Protocol mismatch...\n");
      // TODO: send err msg
    }

    // TODO: send hello resp
    client->state = STATE_MSG;
    printf("Client upgraded to STATE_MSG\n");
  }

  if (client->state == STATE_MSG) {
  }
}

void init_clients(clientstate_t *states) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    states[i].fd = -1; // Initialized to an invalid file descriptor (not used)
    states[i].state = STATE_NEW;
    memset(
        &states[i].buffer, '\0',
        BUFF_SIZE); // Clear the buffer when initialising to make sure its empty
  }
}

// Returns the index of clientStates that is empty/free
// -1 if none is found. All clients are used
int find_free_slot_index(clientstate_t *state) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (state[i].fd == -1) {
      return i;
    }
  }

  return -1;
}

int find_slot_by_fd(clientstate_t *states, int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (states[i].fd == fd) {
      return i;
    }
  }

  return -1;
}
