#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "srvpoll.h"

void fsm_reply_hello(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_HELLO_RESP);
  hdr->len = htons(1);
  dbproto_hello_resp *hello = (dbproto_hello_resp *)&hdr[1];
  hello->proto = htons(PROTO_VER);

  write(client->fd, hdr, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_resp));
}

void fsm_reply_hello_err(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_ERROR);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_add(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_EMPLOYEE_ADD_RESP);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_add_err(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_ERROR);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t *employees,
int fsm_reply_list(clientstate_t *client, dbproto_hdr_t *hdr,
                   struct dbheader_t *dbhdr, struct employee_t **employeesPtr) {
  hdr->type = htonl(MSG_EMPLOYEE_LIST_RESP);
  hdr->len = htons(dbhdr->count);
  write(client->fd, hdr, sizeof(dbproto_hdr_t));

  dbproto_employee_list_resp *emp_resp = (dbproto_employee_list_resp *)&hdr[1];

  struct employee_t *employees = *employeesPtr;

  // Copy the employyes list to the empList header mem space
  for (int i = 0; i < dbhdr->count; i++) {
    strncpy(&emp_resp->name, employees[i].name, sizeof(emp_resp->name));
    strncpy(&emp_resp->address, employees[i].address,
            sizeof(emp_resp->address));
    emp_resp->hours = htonl(employees[i].hours);
    write(client->fd, emp_resp, sizeof(dbproto_employee_list_resp));
  }

  printf("Sending %d employees back to client\n", dbhdr->count);

  return STATUS_SUCCESS;
}

                       clientstate_t *client, int dbfd) {
  dbproto_hdr_t *hdr = (dbproto_hdr_t *)client->buffer;

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  // If the client has not gone through hello, upgrade to hello
  if (client->state == STATE_CONNECTED) {
    printf("Client upgraded to STATE_HELLO\n");
    client->state = STATE_HELLO;
  }

  if (client->state == STATE_HELLO) {
    if (hdr->type != MSG_HELLO_REQ || hdr->len != 1) {
      printf("Client state: HELLO but header type != HELLO_REQ or len !=1\n");
      fsm_reply_hello_err(client, hdr);
      return;
      // TODO: send err msg
    }

    // Move 1 header size in the client->buffer and cast to dbproto_hello_req
    dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
    hello->proto = ntohs(hello->proto);
    if (hello->proto != PROTO_VER) {
      printf("Protocol mismatch...\n");
      fsm_reply_hello_err(client, hdr);
    }

    // Send hello response
    fsm_reply_hello(client, hdr);

    // Upgrade to MSG state
    client->state = STATE_MSG;
    printf("Client upgraded to STATE_MSG\n");
    return;
  }

  if (client->state == STATE_MSG) {
    if (hdr->type == MSG_EMPLOYEE_ADD_REQ) {
      printf("Got add emp req\n");
      dbproto_employee_add_req *employee = (dbproto_employee_add_req *)&hdr[1];

      printf("Adding employee: %s\n", employee->data);

      if (add_employee(dbhdr, &employees, employee->data) != STATUS_SUCCESS) {
        printf("[Reply]: Failed to add employee: %s\n", employee->data);
        fsm_reply_add_err(client, hdr);
      } else {
        printf("Saving output file\n");
        output_file(dbfd, dbhdr, employees);
        fsm_reply_add(client, hdr);
        printf("[Reply]: Successfully added employee\n");
      }
    }
    if (hdr->type == MSG_EMPLOYEE_LIST_REQ) {
      printf("Got list emp req\n");
      if (fsm_reply_list(client, hdr, dbhdr, employees) == STATUS_ERROR) {
        printf("How did we fail to list?\n");
        fsm_reply_err(client, hdr);
      }
    }
  }

  return;
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
