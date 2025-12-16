#include "libstorage.h"
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
}

int main() {
  int sock_fd = connect_to_server();

  const char *test_key = "test_key";
  const uint8_t test_data[] = "Hello, World!";
  uint32_t test_data_len = sizeof(test_data) - 1;
  char *keys[] = {(char *)test_key};

  for (int i = 0; i < REQ_CNT / 2; i++) {
    send_set_request(sock_fd, test_key, 0, 0, test_data, test_data_len);
    send_get_request(sock_fd, keys, 1);
  }

  close(sock_fd);

  return 0;
}
