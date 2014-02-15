/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DAC.
 *
 * Generates the sin/cos of the carrier and then modulates data over SSB.
 */

int16_t sin_table[128];

//IntervalTimer carrier_timer;

static void
periodic_timer_configure(
	uint32_t interval
)
{
	// Enable clock to PIT  
	SIM_SCGC6 |= SIM_SCGC6_PIT;

	// Enable PIT module  
	PIT_MCR = 0;

	// Set re-load value  
	PIT_LDVAL0 = interval-1;  

	// Enable this channel without interrupts  
	PIT_TCTRL0 = 1;

	// Clear timer flag  
	PIT_TFLG0 = 1;
}


static void
periodic_timer_disable(void)
{
	PIT_TCTRL0 = 0;   
}


static int
periodic_timer_expired(void)
{
	int rc = (PIT_TFLG0 & 1) != 0;

	// Clear timer flag if it is set
	if (rc)
		PIT_TFLG0 = 1;

	return rc;
}


void
setup(void)
{
	analogWriteResolution(12);
	analogWrite(A14, 0);
	pinMode(12, OUTPUT);

	for (int i = 0 ; i < 128 ; i++)
		sin_table[i] = sin(i * M_PI / 64) * 2048;

	//carrier_timer.begin(ssb_output, 1.5);

	// Controls the period of the carrier output
	// 50 works out to 60 KHz transmission
	// 16 calls per cycle == 800 ticks @ 48 Mhz == 60 KHz
	// (PIT runs at F_BUS=48, not system clock of 72)
	periodic_timer_configure(100);
}


static inline void
dac_output(
	uint16_t x
)
{
	if (x < 0) x = 0;
	if (x > 4095) x = 4095;
	*(volatile uint16_t*)&(DAC0_DAT0L) = x;
}


static void
ssb_output(
	int16_t ps,
	int16_t pc
)
{
	static uint32_t i = 0;

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

	dac_output(signal);
	i += 16;
}


void
loop(void)
{
	uint32_t sig = 0;
	static uint8_t bytes[] = { 0xA5, 0x00, 0xFF, 0x18, 0xFF };
	int bit_offset = 0;
	const int bit_count = sizeof(bytes) * 8;
	int bit_changed = 1;
	int cur_bit = 0;
	int new_bit = 0;

	while (1)
	{
		// signal sin and cos
		// each loop is 960 KHz, so /2048 == 468 Hz
		const uint16_t ps_i = sig >> 11;
		const uint16_t pc_i = ps_i + 32 + 64;

		int32_t ps = +sin_table[ps_i % 128];
		int32_t pc = -sin_table[pc_i % 128];

#if 1
		// we want 31.250 Hz for our bit clock,
		// 960000 / 31.250 == 30720 cycles
#define ramp_size 8192
#define bit_len 30720

		if (sig == bit_len)
		{
			// time for the next bit
			sig = 0;
			cur_bit = new_bit;
		} else
		if (sig < ramp_size)
		{
			// ramp up if this bit changed
			if (bit_changed)
			{
				int mag = sig;
				ps = (ps * mag) / ramp_size;
				pc = (pc * mag) / ramp_size;
			}
		} else
		if (sig == (bit_len - ramp_size))
		{
			new_bit = bytes[bit_offset / 8];
			new_bit &= 1 << (bit_offset % 8);
			if (new_bit != cur_bit)
				bit_changed = 1;
			else
				bit_changed = 0;
			cur_bit = new_bit;
			bit_offset++;
			if (bit_offset == bit_count)
				bit_offset = 0;
		} else
		if (sig > (bit_len - ramp_size))
		{
			// ramp down if this bit will change
			if (bit_changed)
			{
				int mag = bit_len - sig;
				ps = (ps * mag) / ramp_size;
				pc = (pc * mag) / ramp_size;
			}
		}
#else
		int mag = (sig >> 18) % 256;
		ps = (ps * mag) / 256;
		pc = (pc * mag) / 256;
#endif

		while (!periodic_timer_expired())
			;

		if (cur_bit)
			ssb_output(ps, pc);
		else
			ssb_output(pc, ps);

		sig++;
	}
}
