#ifndef DRIVERS_BLOCK_BLOCK_DEVICE_H
#define DRIVERS_BLOCK_BLOCK_DEVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct block_device block_device_t;

typedef bool (*block_read_fn)(block_device_t *dev, uint64_t lba, uint32_t sector_count, void *buffer);

struct block_device {
    const char *name;
    uint32_t sector_size;
    uint64_t sector_count;
    void *driver_data;
    block_read_fn read;
};

void block_register_device(block_device_t *dev);
block_device_t *block_get_device(size_t index);
size_t block_device_count(void);

#endif
