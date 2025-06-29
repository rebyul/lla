#ifndef SRVPOLL_H
#define SRVPOLL_H

#include "parse.h"

#define MAX_CLIENTS 1024
#define BUFF_SIZE 4096
#define BACKLOG 10

typedef enum { // Connection State enum
  STATE_NEW,
  STATE_CONNECTED,
  STATE_DISCONNECTED,
  STATE_HELLO,
  STATE_MSG,
  STATE_GOODBYE
} state_e;

typedef struct {
  int fd;
  state_e state;
  char buffer[BUFF_SIZE]; // Each client gets their own buffer
} clientstate_t;

void init_clients(clientstate_t *clientStates);
int find_free_slot_index(clientstate_t *state);
int find_slot_by_fd(clientstate_t *states, int fd);
void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t **employees,
                       clientstate_t *client, int dbfd);

#endif
