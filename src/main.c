#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
  printf("Usage: %s -n -f <database files>\n", argv[0]);
  printf("\t -n - create new database file\n");
  printf("\t -f - (required) path to database file\n");

  printf("Options:\n");

  printf("\t -l - list all employees\n");
  printf("\t -a - add new employee: format: {name},{address},{hours}\n");
  printf("\t -r - remove employee by name\n");
  printf("\t -u - update employee by name. New name is given without flags "
         "e.g. -u 'Tom' 'Tim'\n");
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

  while ((c = getopt(argc, argv, "nlf:a:r:u:e:")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case 'a':
      addstring = optarg;
      break;
    case 'l':
      list = true;
      break;
    case 'r':
      removeName = optarg;
      break;
    case 'u':
      emp_to_update_name = optarg;
      break;
    case 'e':
      newHours = atoi(optarg);
      break;
    case '?':
      printf("Unknown option -%c\n", c);
      break;
    default:
      printf("Default main\n");
      return -1;
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
      return -1;
    }

    if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to create database header\n");
      return -1;
    }
  } else {
    dbfd = open_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      printf("Unable to open database file \"%s\"\n", filepath);
    }

    if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to validate database header\n");
      return -1;
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
      return -1;
    }

    printf("Successfully deleted employee %s. New Employee count: %d\n",
           removeName, dbhdr->count);
  }

  if (emp_to_update_name) {
    // Check new name
    if (newHours == -1) {
      printf("New hours must be a positive number: %d", newHours);
      return -1;
    }

    update_employee_hours(dbhdr, &employees, emp_to_update_name, newHours);
    printf("Updated %s hours to %u\n", emp_to_update_name, newHours);
  }

  printf("Newfile: %s\n", newfile ? "true" : "false");
  printf("Filepath: %s\n", filepath);

  /* debug_db_header(dbhdr); */
  output_file(dbfd, dbhdr, employees);

  free(dbhdr);
  dbhdr = NULL;
  free(employees);
  employees = NULL;
  return 0;
}
