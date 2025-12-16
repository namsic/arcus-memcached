#include "libstorage.h"
#include "proto/kv.pb-c.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
  assert(send(sock_fd, buf, len, 0) == len);

  uint32_t resp_len;
  assert(recv(sock_fd, &resp_len, sizeof(resp_len), 0) == sizeof(resp_len));
  uint8_t resp_buf[1024];
  assert(recv(sock_fd, resp_buf, resp_len, 0) == resp_len);
}

int main() {
  int sock_fd = connect_to_server();

  const char *test_key = "test_key";
  uint8_t test_data[] = "Hello, World!";
  uint32_t test_data_len = sizeof(test_data) - 1;
  char *keys[] = {(char *)test_key};

  for (int i = 0; i < REQ_CNT / 2; i++) {
    send_set_request(sock_fd, test_key, 0, 0, test_data, test_data_len);
    send_get_request(sock_fd, keys, 1);
  }

  close(sock_fd);

  return 0;
}
