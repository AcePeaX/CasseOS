#include "drivers/block/block_device.h"
#include "drivers/screen.h"

#define MAX_BLOCK_DEVICES 8

static block_device_t *registered_devices[MAX_BLOCK_DEVICES];
static size_t registered_count = 0;

void block_register_device(block_device_t *dev) {
    if (!dev || registered_count >= MAX_BLOCK_DEVICES) {
        printf("[BLOCK] Failed to register device (limit=%u)\n", MAX_BLOCK_DEVICES);
        return;
    }
    registered_devices[registered_count++] = dev;
    printf("[BLOCK] Registered device '%s' sectors=%llu size=%u\n",
           dev->name ? dev->name : "(unnamed)",
           (unsigned long long)dev->sector_count,
           dev->sector_size);
}

block_device_t *block_get_device(size_t index) {
    if (index >= registered_count) {
        return NULL;
    }
    return registered_devices[index];
}

size_t block_device_count(void) {
    return registered_count;
}
