/** \file
 */
#include <stdio.h>
#include <stdlib.h>
#include "pru.h"

int
main(void)
{
	pru_t * const pru = pru_init(0);

	uint32_t * const pru_cmd = pru->data_ram;
	uint8_t * const vram = pru->ddr;

	//pru_gpio(1, 13, 1, 1);
	//pru_gpio(0, 23, 1, 1);
	//pru_gpio(0, 27, 1, 1);

	pru_exec(pru, "./sdr.bin");
	printf("pru %p\n", pru);
	printf("cmd %p\n", pru_cmd);
	printf("ddr %p (%08x)\n", vram, pru->ddr_addr);

	while (1)
	{
		*pru_cmd = pru->ddr_addr;
		usleep(1000);
	}

	return 0;
}
