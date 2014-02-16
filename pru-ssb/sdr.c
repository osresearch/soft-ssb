/** \file
 * Software defined radio for the BeagleBone Black PRU.
 *
 * Generates a waveform and pokes it to the PRU for output.
 * Requires the userspace to shuffle the bits before handing
 * them to the PRU for output.
 */
#include <stdio.h>
#include <stdlib.h>
#include "pru.h"

static uint16_t
shuffle(
	uint8_t x_in
)
{
	uint16_t x = x_in;
	uint16_t y = 0
	  | (x & 0x0F) << 0
	  | (x & 0x10) << 1
	  | (x & 0x20) << 2
	  | (x & 0xC0) << 6
	  ;

	return y;
}



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

	volatile uint16_t * const buf = pru_cmd;
	while (1)
	{
		for (int i = 0 ; i < 4096 ; i++)
		{
			buf[i] = shuffle(i);
		}

		//*pru_cmd = pru->ddr_addr;
		usleep(1000);
	}

	return 0;
}
