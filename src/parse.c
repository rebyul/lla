#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {}

void parse_employee(char *addstring, struct employee_t **newEmpOut) {
  struct employee_t *newEmp = calloc(1, sizeof(struct employee_t));

  char *name = strtok(addstring, ",");
  // Internally strtok tracks how far we have gone through addstr
  // So we dont need to pass in addstr again
  char *addr = strtok(NULL, ",");
  char *hours = strtok(NULL, ",");

  strncpy(newEmp->name, name, sizeof(newEmp->name) - 1);
  newEmp->name[sizeof(newEmp->name) - 1] = '\0'; // Null termination

  strncpy(newEmp->address, addr, sizeof(newEmp->address) - 1);
  newEmp->address[sizeof(newEmp->address) - 1] = '\0'; // Null termination

  newEmp->hours = atoi(hours);

  *newEmpOut = newEmp;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t *employees,
                 char *addstring) {
  /* printf("%s\n", addstring); */
  /* struct employee_t *newEmp = NULL; */
  struct employee_t **newEmp = malloc(sizeof(struct employee_t *));
  parse_employee(addstring, newEmp);
  /* printf("Double ptr success: %s %s %d\n", (newEmp)->name, (newEmp)->address,
   */
  /*        (newEmp)->hours); */
  /* (*employees)[dbhdr->count] = */
  /*     *newEmp; // Dereference to copy the struct's contents */
  /* struct employee_t **lastEmpSlot = calloc(1, sizeof(struct employee_t)); */
  /* employees[dbhdr->count - 1] = (*newEmp); */
  /* *lastEmpSlot = &employees[dbhdr->count - 1]; */
  /* lastEmpSlot = &newEmp; */
  strncpy(employees[dbhdr->count - 1].name, (*newEmp)->name,
          sizeof(employees[dbhdr->count - 1].name));
  strncpy(employees[dbhdr->count - 1].address, (*newEmp)->address,
          sizeof(employees[dbhdr->count - 1].address));
  employees[dbhdr->count - 1].hours = (*newEmp)->hours;

  /* free(newEmp); */
  /* newEmp = NULL; */
  /* free(lastEmpSlot); */
  /* lastEmpSlot = NULL; */
  return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr,
                   struct employee_t **employeesOut) {
  if (fd < 0) {
    printf("Invalid file descriptor\n");
    return STATUS_ERROR;
  }

  struct employee_t *employees =
      calloc(dbhdr->count, sizeof(struct employee_t));

  if (employees == NULL) {
    printf("Malloc failed to create db header\n");
    free(dbhdr);
    return STATUS_ERROR;
  }

  if (read(fd, employees, dbhdr->count * sizeof(struct employee_t)) == -1) {
    perror("read");
    return STATUS_ERROR;
  };

  for (int i = 0; i < dbhdr->count; i++) {
    employees[i].hours = ntohl(employees[i].hours);
  }

  *employeesOut = employees;

  return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr,
                struct employee_t *employees) {
  if (fd < 0) {
    printf("Invalid file descriptor\n");
    return STATUS_ERROR;
  }
  int realcount = dbhdr->count;

  dbhdr->magic = htonl(dbhdr->magic);
  dbhdr->filesize = htonl(sizeof(struct dbheader_t) +
                          (sizeof(struct employee_t) * realcount));
  dbhdr->count = htons(dbhdr->count);
  dbhdr->version = htons(dbhdr->version);

  lseek(fd, 0, SEEK_SET);

  write(fd, dbhdr, sizeof(struct dbheader_t));

  int i = 0;
  for (; i < realcount; i++) {
    printf("hours: %d", employees[i].hours);
    employees[i].hours = htonl(employees[i].hours);
    write(fd, &employees[i], sizeof(struct employee_t));
  }

  close(fd);

  return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
  if (fd < 0) {
    printf("Invalid file descriptor\n");
    return STATUS_ERROR;
  }

  struct dbheader_t *dbhdr = calloc(1, sizeof(struct dbheader_t));

  if (dbhdr == NULL) {
    printf("Malloc failed to create db header\n");
    free(dbhdr);
    return STATUS_ERROR;
  }

  // When dealing with binary data, make sure it's network endian on disk
  // Then change it to the host's endianness

  if (read(fd, dbhdr, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
    perror("read");
    free(dbhdr);

    return STATUS_ERROR;
  };

  dbhdr->version = ntohs(dbhdr->version);
  dbhdr->count = ntohs(dbhdr->count);
  dbhdr->magic = ntohl(dbhdr->magic);
  dbhdr->filesize = ntohl(dbhdr->filesize);

  if (dbhdr->version != 1) {
    printf("Improper header version\n");
    free(dbhdr);
  }

  if (dbhdr->magic != HEADER_MAGIC) {
    printf("Improper header magic\n");
    free(dbhdr);
  }

  struct stat dbstat = {0};
  fstat(fd, &dbstat);

  if (dbhdr->filesize != dbstat.st_size) {
    printf("Corrupted database\n");
    return STATUS_ERROR;
  }

  *headerOut = dbhdr;
  return STATUS_SUCCESS;
}

int create_db_header(int fd, struct dbheader_t **headerOut) {
  struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));

  if (header == NULL) {
    printf("Malloc failed to create db header\n");
    return STATUS_ERROR;
  }

  header->version = 0x1;
  header->count = 0;
  header->magic = HEADER_MAGIC;
  header->filesize = sizeof(struct dbheader_t);

  if (fd == -1) {
    printf("Invalid file descriptor\n");
    return STATUS_ERROR;
  }

  *headerOut = header;
  printf("%d\n", header->count);

  return STATUS_SUCCESS;
}

void debug_db_header(struct dbheader_t *dbhdr) {
  printf("header version %u\n", dbhdr->version);
  printf("header filesize %u\n", dbhdr->filesize);
  printf("header magic %x\n", dbhdr->magic);
  printf("header count %d\n", dbhdr->count);
}
