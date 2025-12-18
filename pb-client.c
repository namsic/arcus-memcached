#include "libstorage.h"
#include "proto/kv.pb-c.h"
#include <arpa/inet.h>
#include <assert.h>
#include <bits/time.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <time.h>
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
                      int64_t expire_time, uint8_t *data, uint32_t data_len) {
  Kv__SetRequest request = KV__SET_REQUEST__INIT;
  request.key = (char *)key;
  request.flags = flags;
  request.expiretime = expire_time;
  request.data.data = data;
  request.data.len = data_len;

  uint8_t buf[1024];
  int len = 0;

  uint8_t msg_type = 1;
  memcpy(buf + len, &msg_type, sizeof(msg_type));
  len += sizeof(msg_type);

  uint32_t msg_len = kv__set_request__get_packed_size(&request);
  memcpy(buf + len, &msg_len, sizeof(msg_len));
  len += sizeof(msg_len);

  kv__set_request__pack(&request, buf + len);
  len += msg_len;

  assert(send(sock_fd, buf, len, 0) == len);

  uint32_t resp_len;
  assert(recv(sock_fd, &resp_len, sizeof(resp_len), 0) == sizeof(resp_len));
  uint8_t resp_buf[1024];
  assert(recv(sock_fd, resp_buf, resp_len, 0) == resp_len);
}

void send_get_request(int sock_fd, char **keys, size_t num_keys) {
  Kv__GetRequest request = KV__GET_REQUEST__INIT;
  request.keys = keys;
  request.n_keys = num_keys;

  uint8_t buf[1024];
  int len = 0;

  uint8_t msg_type = 2;
  memcpy(buf + len, &msg_type, sizeof(msg_type));
  len += sizeof(msg_type);

  uint32_t msg_len = kv__get_request__get_packed_size(&request);
  memcpy(buf + len, &msg_len, sizeof(msg_len));
  len += sizeof(msg_len);

  kv__get_request__pack(&request, buf + len);
  len += msg_len;
  printf("get_req_len=%d\n", len);
  assert(send(sock_fd, buf, len, 0) == len);

  uint32_t resp_len;
  assert(recv(sock_fd, &resp_len, sizeof(resp_len), 0) == sizeof(resp_len));
  uint8_t resp_buf[1024];
  assert(recv(sock_fd, resp_buf, resp_len, 0) == resp_len);
  printf("get_resp_len=%lu\n", sizeof(resp_len) + resp_len);

  Kv__GetResponse *response =
      kv__get_response__unpack(NULL, resp_len, resp_buf);
  assert(strcmp(response->values[0]->key, "test_key") == 0);
  assert(strncmp((char *)response->values[0]->value->data.data, "Hello, World!",
                 response->values[0]->value->data.len) == 0);
  assert(response->values[0]->value->flags == 0);
  kv__get_response__free_unpacked(response, NULL);
}

int main() {
  int sock_fd = connect_to_server();

  const char *test_key = "test_key";
  uint8_t test_data[] = "Hello, World!";
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
