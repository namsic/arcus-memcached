#include "libstorage.h"
#include <arpa/inet.h>
#include <assert.h>
#include <bits/types/struct_rusage.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"

int connect_to_server() {
  int sock_fd;
  struct sockaddr_in server_addr;

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(sock_fd >= 0);

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  assert(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) > 0);

  assert(connect(sock_fd, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) >= 0);

  return sock_fd;
}

void send_set_request(int sock_fd, const char *key, uint32_t flags,
                      int64_t expire_time, const uint8_t *data,
                      uint32_t data_len) {
  char buf[1024];
  int len =
      sprintf(buf, "set %s %d %ld %d\r\n", key, flags, expire_time, data_len);
  memcpy(buf + len, data, data_len);
  len += data_len;
  len += sprintf(buf + len, "\r\n");

  assert(send(sock_fd, buf, len, 0) == len);
  assert(recv(sock_fd, buf, 1024, 0) == 8 /* STORED\r\n */);
}

void send_get_request(int sock_fd, char **keys, size_t num_keys) {
  char buf[1024];
  int len = sprintf(buf, "get");
  for (int i = 0; i < num_keys; i++) {
    len += sprintf(buf + len, " %s", keys[i]);
  }
  len += sprintf(buf + len, "\r\n");

  assert(send(sock_fd, buf, len, 0) == len);
  assert(recv(sock_fd, buf, 1024, 0) == 41);
  /*
   * VALUE test_key 0 13\r\n
   * Hello, World!\r\n
   * END\r\n
   */
  assert(strncmp(buf, "VALUE ", 6) == 0);
  char *key = buf + 6;
  char *cur = key;
  while (*cur != ' ') {
    cur++;
  }
  *cur = '\0';
  assert(strcmp(key, "test_key") == 0);
  cur++;
  uint32_t flags = strtol(cur, &cur, 10);
  assert(flags == 0);
  assert(*cur == ' ');
  cur++;
  int data_len = strtoul(cur, &cur, 10);
  assert(data_len == 13);
  assert(strncmp(cur, "\r\n", 2) == 0);
  cur += 2;
  assert(strncmp(cur, "Hello, World!\r\nEND\r\n", 20) == 0);
}

int main() {
  int sock_fd = connect_to_server();

  const char *test_key = "test_key";
  const uint8_t test_data[] = "Hello, World!";
  uint32_t test_data_len = sizeof(test_data) - 1;
  char *keys[] = {(char *)test_key};

  struct timespec start, end;
  struct timespec total_start, total_end;
  struct rusage usage_start, usage_end;
  long long set_total_time_ns = 0;
  long long get_total_time_ns = 0;

  getrusage(RUSAGE_SELF, &usage_start);
  clock_gettime(CLOCK_MONOTONIC, &total_start);

  for (int i = 0; i < REQ_CNT / 2; i++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    send_set_request(sock_fd, test_key, 0, 0, test_data, test_data_len);
    clock_gettime(CLOCK_MONOTONIC, &end);
    set_total_time_ns +=
        ((long long)(end.tv_sec - start.tv_sec) * 1000000000LL) +
        (end.tv_nsec - start.tv_nsec);

    clock_gettime(CLOCK_MONOTONIC, &start);
    send_get_request(sock_fd, keys, 1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    get_total_time_ns +=
        ((long long)(end.tv_sec - start.tv_sec) * 1000000000LL) +
        (end.tv_nsec - start.tv_nsec);
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
  printf("Average latency(set): %.2f us\n",
         (set_total_time_ns / 1000.0) / REQ_CNT);
  printf("Average latency(get): %.2f us\n",
         (get_total_time_ns / 1000.0) / REQ_CNT);
  printf("Total CPU time used: %.2f s\n", cpu_time_used / 1000000.0);
  printf("Average CPU usage per second: %.2f%%\n", avg_cpu_per_second * 100);

  close(sock_fd);

  return 0;
}
