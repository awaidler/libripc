#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ripc.h"
#include "config.h"
#include "common.h"

int main(void) {
	uint32_t i;

	ripc_register_service_id(4);
	void **short_items = NULL, **long_items = NULL;
	int length[WORDS_PER_PACKET];
	for (i = 0; i < WORDS_PER_PACKET; ++i)
		length[i] = PACKET_SIZE;
	int num_items, count = 0;
	uint16_t from;

	printf("Starting loop\n");
	while(true) {
		DEBUG("Waiting for message");
		num_items = ripc_receive(4, &from, &short_items,&long_items);
		DEBUG("Received message: %d\n", *(int *)long_items[0]);
		//printf("pingpong %d\n", ++count);
		ripc_send_long(4, from, long_items, length, WORDS_PER_PACKET);
		for (i = 0; i < WORDS_PER_PACKET; ++i)
			ripc_buf_free(long_items[i]); //returns receive buffer to pool
		free(long_items); //frees the array itself
	}
	return EXIT_SUCCESS;
}
