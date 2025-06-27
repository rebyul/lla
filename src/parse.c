#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

void print_employee(int i, struct employee_t *employee) {
  printf("Employee %d\n", i);
  printf("\t Name: %s\n", employee->name);
  printf("\t Address: %s\n", employee->address);
  printf("\t Hours: %u\n", employee->hours);
}

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {
  for (int i = 0; i < dbhdr->count; i++) {
    print_employee(i, &employees[i]);
  }
}

void update_employee_from_string(char *addstring, struct employee_t *newEmp) {
  char *addstring_copy = strdup(addstring);

  if (addstring_copy == NULL) {
    perror("strdup failed to copy addstring");
    return;
  }

  char *name = strtok(addstring_copy, ",");
  // Internally strtok tracks how far we have gone through addstr
  // So we dont need to pass in addstr again
  char *addr = strtok(NULL, ",");
  char *hours = strtok(NULL, ",");

  strncpy((newEmp)->name, name, sizeof((newEmp)->name) - 1);
  (newEmp)->name[sizeof((newEmp)->name) - 1] = '\0'; // Null termination

  strncpy((newEmp)->address, addr, sizeof((newEmp)->address) - 1);
  (newEmp)->address[sizeof((newEmp)->address) - 1] = '\0'; // Null termination

  (newEmp)->hours = atoi(hours);

  free(addstring_copy);
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employeesOut,
                 char *addstring) {
  struct employee_t *new_employees =
      realloc(*employeesOut, (dbhdr->count + 1) * sizeof(struct employee_t));

  if (new_employees == NULL) {
    perror("Realloc new_employees failed");
    return STATUS_ERROR;
  };

  // If realloc succeeded, assign the new block to our pointer
  *employeesOut = new_employees;

  // set newEmp to point to the new slot allocated in employeesOut
  struct employee_t *newEmp = &((*employeesOut)[dbhdr->count]);

  if (!newEmp) {
    perror("malloc");
    printf("Failed to malloc **newEmp\n");
    free(newEmp);
    return STATUS_ERROR;
  }

  update_employee_from_string(addstring, newEmp);

  // Only increment the header count after everything succeeds
  dbhdr->count++;

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
    free(dbhdr);
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
  // Gotta save this before it's changed from htonl (network long)
  int hostCountValue = dbhdr->count;
  unsigned int final_file_size =
      sizeof(struct dbheader_t) + (sizeof(struct employee_t) * dbhdr->count);

  dbhdr->magic = htonl(dbhdr->magic);
  dbhdr->filesize = htonl(final_file_size);
  dbhdr->count = htons(dbhdr->count);
  dbhdr->version = htons(dbhdr->version);

  lseek(fd, 0, SEEK_SET);

  write(fd, dbhdr, sizeof(struct dbheader_t));

  int i = 0;
  for (; i < hostCountValue; i++) {
    employees[i].hours = htonl(employees[i].hours);
    write(fd, &employees[i], sizeof(struct employee_t));
  }

  if (ftruncate(fd, final_file_size) == -1) {
    perror("ftruncate failed");
    return STATUS_ERROR;
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
    debug_db_header(dbhdr);
    printf("Corrupted database. filesize: %u, st_size: %lld\n", dbhdr->filesize,
           dbstat.st_size);
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

int remove_employee_by_name(struct dbheader_t *dbhdr,
                            struct employee_t **employees, char *name) {
  // Find employee index to remove
  int toRemoveIndex = -1;

  for (unsigned int j = 0; j < dbhdr->count; j++) {
    unsigned int equality = strcmp((*employees)[j].name, name);

    // If name matches
    if (equality == 0) {
      toRemoveIndex = j;
      break;
    }
  }

  // If not found
  if (toRemoveIndex == -1) {
    printf("Employee: %s was not found. Can't remove\n", name);
    return STATUS_ERROR;
  }

  printf("Found emp at index: %d\n", toRemoveIndex);

  // Find how many employees there exists after the employees list
  int employees_to_move = dbhdr->count - toRemoveIndex - 1;

  if (employees_to_move > 0) {
    memmove(&((*employees)[toRemoveIndex]), &((*employees)[toRemoveIndex + 1]),
            employees_to_move * sizeof(struct employee_t));
  }

  // If we removed the last item, free the employees list
  if ((dbhdr->count - 1) == 0) {
    printf("No employees left. Freeing employees and return success\n");
    free(*employees);
    *employees = NULL;
  }
  // Else we want to resize the employees by moving the structs that exist after
  // the employee to delete forward one index
  else {
    struct employee_t *removed_employees =
        realloc(*employees, (dbhdr->count - 1) * sizeof(struct employee_t));

    if (removed_employees == NULL) {
      free(removed_employees);
      perror("realloc to shrink failed");
      return STATUS_ERROR;
    }

    *employees = removed_employees;
  }

  // Decrease header count after everything is done
  dbhdr->count--;

  return STATUS_SUCCESS;
}

void debug_db_header(struct dbheader_t *dbhdr) {
  printf("Header: version %u\n", dbhdr->version);
  printf("Header: filesize %u\n", dbhdr->filesize);
  printf("Header: magic %x\n", dbhdr->magic);
  printf("Header: count %d\n", dbhdr->count);
}
