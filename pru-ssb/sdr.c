/** \file
 * Software defined radio for the BeagleBone Black PRU.
 *
 * Generates a waveform and pokes it to the PRU for output.
 * Requires the userspace to shuffle the bits before handing
 * them to the PRU for output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pru.h"

static int sin_table[128];

static uint16_t
shuffle(
	uint8_t x_in
)
{
	uint16_t x = x_in >> 1; // ignore bottom bit; only have 7 wired
	uint16_t y = 0
	  | (x & 0x0F) << 0
	  | (x & 0x10) << 1
	  | (x & 0x20) << 2
	  | (x & 0xC0) << 8
	  ;

	return y;
}


static const uint8_t psk_bits[] = ""
"1010110100" // C
"11101110100" // Q
"100" // space
"1010110100" // C
"11101110100" // Q
"100" // space
"1101110100" // N
"10111101100" // Y
"1111111100" // 3
"10101011100" // U
"100" // space
;


/** Output a PSK31 encoded bit.
 * Phase controls which sin/cos is used.
 * ramp_up will ramp up over a few ms
 * ramp_down will ramp down over a few ms
 */
static void
generate_bit(
	uint16_t * const buf,
	int phase,
	int ramp_up,
	int ramp_down
)
{
	// 32 ms per bit == 313 passes at 102.4 usec per pass
	for (unsigned j = 0 ; j < 313 ; j++)
	{
		int mag = 1024;

		if (ramp_up && j < 128)
			mag = sin_table[j/4];
		if (ramp_down && j > 313 - 128)
			mag = sin_table[(313 - j - 1)/4];

		for (unsigned i = 0 ; i < 4096 ; i++)
		{
			// 25 ns per output, 65536 outputs per cycle
			// 1638400 ns per cycle == 610 Hz
			int sig_ti = ((j << 12) + i) / 65536;
			if (phase)
				sig_ti += 64;

			int sig_sin = +sin_table[(sig_ti +  0) % 128];
			int sig_cos = -sin_table[(sig_ti + 32) % 128];

			// 25 ns per output, 16 outputs per cycle
			// 400 ns per cycle == 2.5 MHz
			// Carrier frequency == 1e9 / (25 ns * (128 / N))
			// f = 1e9 * N / (128 * 25)
			// N = f * 128 * 25 / 1e9
			int c_ti = i*8;
			int c_sin = +sin_table[(c_ti +  0) % 128];
			int c_cos = -sin_table[(c_ti + 32) % 128];

			// 102.4 us per pass
			// 512 Hz = 1953 us
			int ssb = (c_sin * sig_cos - c_cos * sig_sin) / 1024;

			// if we're ramping, scale it
			ssb = (ssb * mag) / 1024;

			// ssb has range -1024 to 1024;
			// convert to 0 to 256
			buf[4096*j + i] = shuffle(ssb / 8 + 127);

			// mark an unused bit; the pru will zero
			// this when it is halfway through the buffer.
		}
	}
}


static void
output_bit(
	volatile uint16_t * const pru_buf,
	const uint16_t * bit_buf
)
{
	// send every 3rd section.  This keeps up with the correct
	// bit timings.  total hack until we figure out a faster
	// way to send the data to the PRU.
	for (unsigned i = 0 ; i < 313 ; i += 3)
	{
		// wait for a signal that we're halfway
		// so that buf is free.
		while((pru_buf[0] & (1<<8)) != 0)
			;

		memcpy(pru_buf, &bit_buf[i*4096], 8192);

		pru_buf[0] |= (1 << 8) | (1 << 15);
	}
}


int
main(void)
{
	for (int i = 0 ; i < 128 ; i++)
	{
		sin_table[i] = sin(i * M_PI * 2 / 128) * 1024;
	}

	uint16_t * bits[8];

	for (int i = 0 ; i < 8 ; i++)
	{
		bits[i] = calloc(2, 4096 * 313);
		if (bits[i] == NULL)
			fprintf(stderr, "failed\n");
	}


	// we need bit streams for:
	// phase 0 ramp up
	// phase 0 solid
	// phase 0 ramp down
	// phase 1 ramp up
	// phase 1 solid
	// phase 1 ramp down
	generate_bit(bits[0], 0, 0, 0);
	generate_bit(bits[1], 0, 0, 1);
	generate_bit(bits[2], 0, 1, 0);
	generate_bit(bits[3], 0, 1, 1);
	generate_bit(bits[4], 1, 0, 0);
	generate_bit(bits[5], 1, 0, 1);
	generate_bit(bits[6], 1, 1, 0);
	generate_bit(bits[7], 1, 1, 1);

	pru_t * const pru = pru_init(0);

	uint32_t * const pru_cmd = pru->data_ram;
	uint8_t * const vram = pru->ddr;

	pru_exec(pru, "./sdr.bin");
	printf("pru %p\n", pru);
	printf("cmd %p\n", pru_cmd);
	printf("ddr %p (%08x)\n", vram, pru->ddr_addr);

	volatile uint16_t * const buf = (void*) pru_cmd;

	int offset = 0;
	const size_t len = sizeof(psk_bits) - 1;
	int old_bit = 0;
	int cur_bit = 0;
	int new_bit = 0;
	int phase = 0;

	while (1)
	{
		offset = (offset + 1) % len;
		old_bit = cur_bit;
		cur_bit = new_bit;
		new_bit = psk_bits[offset] == '1';

		int ramp_up = 0;
		int ramp_down = 0;

		if (cur_bit == 0)
		{
			phase = !phase;
			ramp_up = 1;
		}

		if (new_bit == 0)
			ramp_down = 1;


		output_bit(buf, bits[ 0
			| phase << 2
		       	| ramp_up << 1
			| ramp_down << 0
		]);
	}

	return 0;
}
