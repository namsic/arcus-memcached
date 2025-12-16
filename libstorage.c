#include "libstorage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_TABLE_SIZE 512

typedef struct hash_node {
  char key[64];
  storage_value_t value;
  struct hash_node *next;
} hash_node_t;

static hash_node_t *hash_table[HASH_TABLE_SIZE];

static unsigned int hash(const char *key) {
  unsigned int hash = 5381;
  int c;
  while ((c = *key++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash % HASH_TABLE_SIZE;
}

int storage_init(void) {
  memset(hash_table, 0, sizeof(hash_table));
  return 0;
}

void storage_cleanup(void) {
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hash_node_t *node = hash_table[i];
    while (node) {
      hash_node_t *next = node->next;
      free(node);
      node = next;
    }
    hash_table[i] = NULL;
  }
}

int storage_set(const char *key, const uint8_t *data, size_t len,
                uint32_t flags, int64_t expiretime) {
  if (!key || !data) {
    return -1;
  }

  unsigned int idx = hash(key);
  hash_node_t *node = hash_table[idx];
  hash_node_t *prev = NULL;

  while (node) {
    if (strcmp(node->key, key) == 0) {
      memcpy(node->value.data, data, len);
      node->value.len = len;
      node->value.flags = flags;
      node->value.expiretime = expiretime;
      return 0;
    }
    prev = node;
    node = node->next;
  }

  node = malloc(sizeof(hash_node_t));
  if (!node) {
    return -1;
  }

  sprintf(node->key, "%s", key);
  memcpy(node->value.data, data, len);
  node->value.len = len;
  node->value.flags = flags;
  node->value.expiretime = expiretime;
  node->next = NULL;

  if (prev) {
    prev->next = node;
  } else {
    hash_table[idx] = node;
  }

  return 0;
}

storage_value_t *storage_get(const char *key) {
  if (!key) {
    return NULL;
  }

  unsigned int idx = hash(key);
  hash_node_t *node = hash_table[idx];

  while (node) {
    if (strcmp(node->key, key) == 0) {
      storage_value_t *value = malloc(sizeof(storage_value_t));
      if (!value) {
        return NULL;
      }

      memcpy(value, &node->value, sizeof(storage_value_t));
      value->len = node->value.len;
      value->flags = node->value.flags;
      value->expiretime = node->value.expiretime;
      return value;
    }
    node = node->next;
  }

  return NULL;
}
