#include "libstorage.h"
#include <arpa/inet.h>
#include <assert.h>
#include <bits/time.h>
#include <bits/types/struct_rusage.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  char *value;
  int length;
} token_t;

void handle_set_request(int sock_fd, char *key, int flags, int exptime,
                        uint8_t *data, int data_len) {

  int result = storage_set(key, data, data_len, flags, exptime);

  char buf[1024];
  int len = 0;
  if (result == 0) {
    len += sprintf(buf, "STORED\r\n");
  } else {
    len += sprintf(buf, "ERROR\r\n");
  }

  assert(send(sock_fd, buf, len, 0) == len);
}

void handle_get_request(token_t *tokens, int ntokens, int client_fd) {
  char buf[1024];
  int len = 0;

  for (int i = 1; i < ntokens - 1; i++) {
    char *key = tokens[i].value;
    storage_value_t *value = storage_get(key);

    if (value != NULL) {
      len += sprintf(buf + len, "VALUE %s %u %zu\r\n", key, value->flags,
                     value->len);
      memcpy(buf + len, value->data, value->len);
      len += value->len;
      len += sprintf(buf + len, "\r\n");
      free(value);
    }
  }

  len += sprintf(buf + len, "END\r\n");
  assert(send(client_fd, buf, len, 0) == len);
}

int tokenize_command(char *command, int cmdlen, token_t *tokens) {
  char *s = command;
  char *e = NULL;
  int ntokens = 0;
  int checked = 0;

  while (1) {
    e = memchr(s, ' ', cmdlen - checked);
    if (e) {
      if (s != e) {
        tokens[ntokens].value = s;
        tokens[ntokens].length = e - s;
        ntokens++;
        *e = '\0';
      }
      s = (++e);
      checked = s - command;
    } else {
      e = command + cmdlen;
      if (s != e) {
        tokens[ntokens].value = s;
        tokens[ntokens].length = e - s;
        ntokens++;
      }
      break;
    }
  }

  assert(*e == '\0');
  tokens[ntokens].value = NULL;
  tokens[ntokens].length = 0;
  return ++ntokens;
}

int main() {
  assert(storage_init() == 0);

  int server_fd, client_fd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(server_fd >= 0);
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(8080);
  assert(bind(server_fd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) >= 0);
  assert(listen(server_fd, 5) >= 0);
  client_fd =
      accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
  assert(client_fd >= 0);

  struct timespec start, end;
  struct timespec total_start, total_end;
  struct rusage usage_start, usage_end;
  long long total_time_ns = 0;

  getrusage(RUSAGE_SELF, &usage_start);
  clock_gettime(CLOCK_MONOTONIC, &total_start);

  for (int i = 0; i < REQ_CNT; i++) {
    char buf[1024];
    int read_len = recv(client_fd, buf, 1024, 0);

    clock_gettime(CLOCK_MONOTONIC, &start);

    token_t tokens[30];
    int ntokens;
    int j;
    for (j = 0; j < read_len; j++) {
      if (buf[j] == '\r' && buf[j + 1] == '\n') {
        buf[j] = '\0';
        ntokens = tokenize_command(buf, j, tokens);
        break;
      }
    }

    if (strcmp(tokens[0].value, "set") == 0) {
      char *key = tokens[1].value;
      uint32_t flags = strtol(tokens[2].value, NULL, 10);
      int64_t exptime = strtol(tokens[3].value, NULL, 10);
      uint8_t *data = (uint8_t *)buf + j + 2;
      int data_len = strtoul(tokens[4].value, NULL, 10);
      handle_set_request(client_fd, key, flags, exptime, data, data_len);
    } else if (strcmp(tokens[0].value, "get") == 0) {
      handle_get_request(tokens, ntokens, client_fd);
    } else {
      assert(0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long long elapsed_ns =
        ((long long)(end.tv_sec - start.tv_sec) * 1000000000LL) +
        (end.tv_nsec - start.tv_nsec);
    total_time_ns += elapsed_ns;
  }

  clock_gettime(CLOCK_MONOTONIC, &total_end);
  getrusage(RUSAGE_SELF, &usage_end);

  int total_elapsed_time = (int)(total_end.tv_sec - total_start.tv_sec);

  long long cpu_time_used =
      (usage_end.ru_utime.tv_sec - usage_start.ru_utime.tv_sec) * 1000000LL +
      (usage_end.ru_utime.tv_usec - usage_start.ru_utime.tv_usec) +
      (usage_end.ru_stime.tv_sec - usage_start.ru_stime.tv_sec) * 1000000LL +
      (usage_end.ru_stime.tv_usec - usage_start.ru_stime.tv_usec);

  double avg_cpu_per_second = (cpu_time_used / 1000000.0) / total_elapsed_time;

  printf("Total request count: %d\n", REQ_CNT);
  printf("Total elapsed time: %ds\n", total_elapsed_time);
  printf("Average throuthput: %d\n", REQ_CNT / total_elapsed_time);
  printf("Average processing time: %.2f us\n",
         (total_time_ns / 1000.0) / REQ_CNT);
  printf("Total CPU time used: %.2f s\n", cpu_time_used / 1000000.0);
  printf("Average CPU usage per second: %.2f%%\n", avg_cpu_per_second * 100);

  close(client_fd);
  close(server_fd);
  storage_cleanup();
  return 0;
}
