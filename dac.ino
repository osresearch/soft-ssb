/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DAC.
 *
 * Generates the sin/cos of the carrier and then modulates data over SSB.
 */

int16_t sin_table[128];

void
setup(void)
{
	analogWriteResolution(12);
	analogWrite(A14, 0);
	pinMode(12, OUTPUT);

	for (int i = 0 ; i < 128 ; i++)
		sin_table[i] = sin(i * M_PI / 64) * 2048;
}


static inline void
dac_output(
	uint16_t x
)
{
	//if (x < 0) x = 0;
	//if (x > 4096) x = 4096;
	*(volatile uint16_t*)&(DAC0_DAT0L) = x;
}


static void
ssb_output(
	int16_t ps,
	int16_t pc
)
{
	static uint32_t i = 0;
	static uint32_t old_usec = 0;

	while (1)
	{
		uint32_t now = (SYST_CVR & 0xFFFFFF) >> 6;
		if (old_usec == now)
			continue;
		old_usec = now;
		break;
	}

	const uint16_t p = i % 128;
	if (p == 0 || p == 64)
		digitalWriteFast(12, p == 0);

	// carrier sin and cos
	int32_t c = sin_table[p];
	int32_t s = sin_table[(p + 64 + 32) % 128];

#if 1
	// SSB
	int32_t signal = (s * ps - c * pc) / 2048;
#else
	// AM
	int32_t signal = c * pc / 2048;
	//int32_t signal = s;
#endif
	signal += 2048; // shift to zero offset
	//signal /= 2;


	dac_output(signal);
	i += 8;
}


void
loop(void)
{
	uint32_t sig = 0;

	while (1)
	{
		// signal sin and cos
		const uint16_t ps_i = sig >> 10;
		const uint16_t pc_i = ps_i + 32 + 64;

		int32_t ps = +sin_table[ps_i % 128];
		int32_t pc = -sin_table[pc_i % 128];

		ssb_output(ps, pc);
		sig += 5;
	}
}
