/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DAC.
 *
 * Generates a 1 MHz carrier and then modulates data over SSB.
 */

uint16_t sin_table[128];

void
setup(void)
{
	analogWriteResolution(12);
	analogWrite(A14, 0);

	for (int i = 0 ; i < 128 ; i++)
		sin_table[i] = (1+sin(i * M_PI / 64)) * 512;
}


static inline void
dac_output(
	uint16_t x
)
{
	*(volatile uint16_t*)&(DAC0_DAT0L) = x;
}


void
loop(void)
{
	unsigned i = 0;
	uint32_t sig = 0;

	while(1)
	{
		uint16_t carrier = sin_table[i % 128];
		uint16_t signal = sin_table[(sig / 65536) % 128];
		dac_output(carrier * signal / 512);
		i += 16;
		sig += 1;
	}
}
