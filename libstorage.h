#ifndef LIBSTORAGE_H
#define LIBSTORAGE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define REQ_CNT 100000

typedef struct {
  uint8_t data[128];
  size_t len;
  uint32_t flags;
  int64_t expiretime;
} storage_value_t;

int storage_init(void);
void storage_cleanup(void);

int storage_set(const char *key, const uint8_t *data, size_t len,
                uint32_t flags, int64_t expiretime);

storage_value_t *storage_get(const char *key);

#endif
