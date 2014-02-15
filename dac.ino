/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DAC.
 *
 * Generates a 1 MHz carrier and then modulates data over SSB.
 */

int16_t sin_table[128];

void
setup(void)
{
	analogWriteResolution(12);
	analogWrite(A14, 0);

	for (int i = 0 ; i < 128 ; i++)
		sin_table[i] = sin(i * M_PI / 64) * 1024;
}


static inline void
dac_output(
	uint16_t x
)
{
	if (x < 0) x = 0;
	if (x > 4096) x = 4096;
	*(volatile uint16_t*)&(DAC0_DAT0L) = x;
}


void
loop(void)
{
	uint32_t i = 0;

	while(1)
	{
		const int32_t c = sin_table[i % 128];
		const int32_t s = sin_table[(i + 32) % 128];

		const uint16_t ps_i = i >> 11;
		const uint16_t pc_i = ps_i + 32;

		int32_t ps = sin_table[ps_i % 128];
		int32_t pc = sin_table[pc_i % 128];
		if (i & 0x1000000)
			ps = pc = 0;

		int32_t signal = (c * pc - s * pc) / 1024;
		signal += 2048; // shift to zero offset
		signal /= 2;


		dac_output(signal);
		i += 16;
	}
}
