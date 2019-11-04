#include <linux/slab.h>
#include <linux/spinlock.h>

typedef struct {
	uint8_t *buffer;
	uint8_t size;
	uint8_t idIn;
	uint8_t idOut;
	uint8_t nbElement;
	spinlock_t buffer_lock;
}buffer;

static void init_buffer(uint8_t size, buffer *buff);
static void read_buffer(uint8_t* tempo, buffer *buff);
static void write_buffer(uint8_t tempo, buffer *buff);
static int resize_buffer(buffer *buffrx, buffer *bufftx, size_t newSize);
static int get_buffer_size(buffer *buff);


