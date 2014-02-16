/** \file
 * Software defined radio for the BeagleBone Black PRU.
 *
 * Generates a waveform and pokes it to the PRU for output.
 * Requires the userspace to shuffle the bits before handing
 * them to the PRU for output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pru.h"

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


static const uint8_t bits[] = ""
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

#if 0
	int bit_offset = 0;
	const int bit_count = sizeof(bits);
	int do_ramp = 1;
	int cur_bit = 0;
	int new_bit = 0;
	int phase = 0;

	while (1)
	{
#if 1
		// we want 31.250 Hz for our bit clock,
		// 960000 / 31.250 == 30720 cycles
#define ramp_size 4096
#define bit_len 16384

		if (sig == bit_len)
		{
			// time for the next bit
			if (do_ramp)
				phase ^= 1;

			sig = 0;
			cur_bit = new_bit;
			//digitalWriteFast(12, phase == 0);
		}

		// signal sin and cos
		uint16_t ps_i = sig >> 3;
		if (phase)
			ps_i += 64;

		uint16_t pc_i = ps_i + 32;

		int32_t ps = +sin_table[ps_i % 128];
		int32_t pc = -sin_table[pc_i % 128];

if (1)
{
		if (sig < ramp_size)
		{
			// ramp up if this bit is unchanged
			if (do_ramp)
			{
				int32_t mag = sig;
				ps = (ps * mag) / ramp_size;
				pc = (pc * mag) / ramp_size;
			}
		}
		if (sig == (bit_len - ramp_size))
		{
			new_bit = bits[bit_offset] == '1';
			if (new_bit == 0)
				do_ramp = 1;
			else
				do_ramp = 0;
			bit_offset++;
			if (bit_offset == bit_count)
				bit_offset = 0;
		}
		if (sig > (bit_len - ramp_size))
		{
			// ramp down if this bit will not change
			if (do_ramp)
			{
				int32_t mag = bit_len - sig;
				ps = (ps * mag) / ramp_size;
				pc = (pc * mag) / ramp_size;
			}
		}
}
#else
		int mag = (sig >> 18) % 256;
		ps = (ps * mag) / 256;
		pc = (pc * mag) / 256;
#endif

#endif

static void
ssb_output(
	volatile uint16_t * const buf,
	float s_sin,
	float s_cos
)
{
	// wait for a signal that we're halfway
	// so that buf is free.  this should go before, not after
	while((buf[0] & (1<<8)) != 0)
		;

	for (int i = 0 ; i < 4096 ; i++)
	{
		// 25 ns per output, 16 outputs per cycle
		// 400 ns per cycle == 2.5 MHz
		float t = 256*i*M_PI/2048;
		float c_sin = sin(t);
		float c_cos = cos(t);

		// 102.4 us per pass
		// 512 Hz = 1953 us
		float ssb = c_sin * s_cos - c_cos * s_sin;

		buf[i] = shuffle(ssb*128 + 127);
	}

	// mark an unused bit; the pru will zero this when it is halfway
	// through the buffer.
	buf[0] |= (1 << 8) | (1 << 15);

}


int
main(void)
{
	pru_t * const pru = pru_init(0);

	uint32_t * const pru_cmd = pru->data_ram;
	uint8_t * const vram = pru->ddr;

	pru_exec(pru, "./sdr.bin");
	printf("pru %p\n", pru);
	printf("cmd %p\n", pru_cmd);
	printf("ddr %p (%08x)\n", vram, pru->ddr_addr);

	volatile uint16_t * const buf = pru_cmd;
	memset(buf, 0, 8192);

	int pass = 10;

	uint32_t t = 0;

	while (1)
	{
		float mag = (pass % 256) / 256.0;
		float s_sin = +sin(pass*M_PI/10) * mag;
		float s_cos = -cos(pass*M_PI/10) * mag;

		// send 102 usec worth of data
		ssb_output(buf, s_sin, s_cos);

		pass++;
	}

	return 0;
}
