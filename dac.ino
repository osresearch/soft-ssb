/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DAC.
 *
 * Generates the sin/cos of the carrier and then modulates data over SSB.
 *
 * We want to generate 16384 passes per bit, 
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
	// PIT runs at F_BUS=48, not system clock of 72 or 96
	// 48 MHz / (94 * 8 cycles) == 63.83 KHz transmission
	// 16384 passes through the loop is 32 ms == 31.25 Hz
	periodic_timer_configure(94);
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


/*
// CQ NY3U
1010110100 C
11101110100 Q
1010110100 C
11101110100 Q
100
1101110100	116	78	4E	N
10111101100	131	89	59	Y
1111111100	063	51	33	3
10101011100	125	85	55	U

10101101
00111011
10100101
01101001
11011101
00100110
11101001
01111011
00111111
11001010
10111000
*/

static const uint8_t bytes[] = {
0xAD,
0x3B,
0xA5,
0x69,
0xDD,
0x26,
0xE9,
0x7B,
0x3F,
0xCA,
0xB8,
};

void
loop(void)
{
	uint32_t sig = 0;
	int bit_offset = 0;
	const int bit_count = sizeof(bytes) * 8;
	int do_ramp = 1;
	int cur_bit = 0;
	int new_bit = 0;

	while (1)
	{
		// signal sin and cos
		const uint16_t ps_i = sig >> 3;
		const uint16_t pc_i = ps_i + 32 + 64;

		int32_t ps = +sin_table[ps_i % 128];
		int32_t pc = -sin_table[pc_i % 128];

#if 1
		// we want 31.250 Hz for our bit clock,
		// 960000 / 31.250 == 30720 cycles
#define ramp_size 2048
#define bit_len 16384

		if (sig == bit_len)
		{
			// time for the next bit
			sig = 0;
			cur_bit = new_bit;
		}

if (1)
{
		if (sig < ramp_size)
		{
			// ramp up if this bit is unchanged
			if (!do_ramp)
			{
				int32_t mag = sig;
				ps = (ps * mag) / ramp_size;
				pc = (pc * mag) / ramp_size;
			}
		}
		if (sig == (bit_len - ramp_size))
		{
			new_bit = bytes[bit_offset / 8];
			new_bit >>= 7 - (bit_offset % 8);
			new_bit &= 1;
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
			if (!do_ramp)
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

		while (!periodic_timer_expired())
			;

#if 0
		dac_output(ps/2 + 2048);
#else
		if (do_ramp && cur_bit)
			ssb_output(ps, pc);
		else
			ssb_output(pc, ps);
#endif

		sig++;
	}
}
