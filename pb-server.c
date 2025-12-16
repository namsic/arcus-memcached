#include "libstorage.h"
#include "proto/kv.pb-c.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void handle_set_request(int sock_fd, uint8_t *req_buf, uint32_t req_len) {
  Kv__SetRequest *request = kv__set_request__unpack(NULL, req_len, req_buf);
  assert(request != NULL);

  Kv__SetResponse response = KV__SET_RESPONSE__INIT;

  int result = storage_set(request->key, request->data.data, request->data.len,
                           request->flags, request->expiretime);

  response.status =
      (result == 0) ? KV__STATUS_CODE__STORED : KV__STATUS_CODE__ERROR;

  uint8_t buf[1024];
  int len = 0;
  uint32_t msg_len = kv__set_response__get_packed_size(&response);
  memcpy(buf + len, &msg_len, sizeof(msg_len));
  len += sizeof(msg_len);
  kv__set_response__pack(&response, buf + len);
  len += msg_len;

  assert(send(sock_fd, buf, len, 0) == len);
  kv__set_request__free_unpacked(request, NULL);
}

void handle_get_request(int sock_fd, uint8_t *req_buf, uint32_t req_len) {
  Kv__GetRequest *request = kv__get_request__unpack(NULL, req_len, req_buf);
  assert(request != NULL);

  Kv__GetResponse response = KV__GET_RESPONSE__INIT;
  Kv__Value values[request->n_keys];
  Kv__GetResponse__ValuesEntry entries[request->n_keys];
  Kv__GetResponse__ValuesEntry *entry_ptrs[request->n_keys];
  storage_value_t *stored_values[request->n_keys];

  size_t found_count = 0;
  for (size_t i = 0; i < request->n_keys; i++) {
    stored_values[i] = storage_get(request->keys[i]);
    if (stored_values[i] != NULL) {
      kv__value__init(&values[found_count]);
      values[found_count].data.data = stored_values[i]->data;
      values[found_count].data.len = stored_values[i]->len;
      values[found_count].flags = stored_values[i]->flags;

      kv__get_response__values_entry__init(&entries[found_count]);
      entries[found_count].key = request->keys[i];
      entries[found_count].value = &values[found_count];

      entry_ptrs[found_count] = &entries[found_count];
      found_count++;
    }
  }

  response.n_values = found_count;
  response.values = entry_ptrs;

  uint8_t buf[1024];
  int len = 0;
  uint32_t msg_len = kv__get_response__get_packed_size(&response);
  memcpy(buf + len, &msg_len, sizeof(msg_len));
  len += sizeof(msg_len);
  kv__get_response__pack(&response, buf + len);
  len += msg_len;

  assert(send(sock_fd, buf, len, 0) == len);

  kv__get_request__free_unpacked(request, NULL);
  for (size_t i = 0; i < request->n_keys; i++) {
    if (stored_values[i] != NULL) {
      free(stored_values[i]);
    }
  }
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
  long long total_time_ns = 0;

  for (int i = 0; i < REQ_CNT; i++) {
    uint8_t msg_type;
    uint32_t msg_len;
    uint8_t msg_buf[1024];

    assert(recv(client_fd, &msg_type, sizeof(msg_type), 0) == sizeof(msg_type));
    assert(recv(client_fd, &msg_len, sizeof(msg_len), 0) == sizeof(msg_len));
    assert(recv(client_fd, msg_buf, msg_len, 0) == msg_len);

    clock_gettime(CLOCK_MONOTONIC, &start);

    if (msg_type == 1) {
      handle_set_request(client_fd, msg_buf, msg_len);
    } else if (msg_type == 2) {
      handle_get_request(client_fd, msg_buf, msg_len);
    } else {
      assert(0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long long elapsed_ns =
        ((long long)(end.tv_sec - start.tv_sec) * 1000000000LL) +
        (end.tv_nsec - start.tv_nsec);
    total_time_ns += elapsed_ns;
  }

  double avg_time_us = (total_time_ns / 1000.0) / REQ_CNT;
  printf("Average time per request: %.2f microseconds\n", avg_time_us);

  close(client_fd);
  close(server_fd);
  storage_cleanup();
  return 0;
}
