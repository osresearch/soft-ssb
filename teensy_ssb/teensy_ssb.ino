/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DMA engine
 * to drive an external DAC connected to PORTD.
 *
 * Good reference to the DMA engine: http://forum.pjrc.com/threads/23950-Parallel-GPIO-on-Teensy-3-0
 * GPIO mappings on the Teensy 3.1: http://forum.pjrc.com/threads/17532-Tutorial-on-digital-I-O-ATMega-PIN-PORT-DDR-D-B-registers-vs-ARM-GPIO_PDIR-_PDOR
 */

//uint8_t sin_table[30000];
uint8_t ramp_up_table[32768];

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

#define LED_PIN 13
static inline void
led(int val)
{
	digitalWrite(LED_PIN, val ? HIGH : LOW);
}


void
setup(void)
{
	pinMode(LED_PIN, OUTPUT);
	led(1);

	for (int i = 0 ; i < sizeof(ramp_up_table) ; i++)
	{
		//float s = sin(1024*i*M_PI/sizeof(sin_table));
		//float s = cos(8*i*2*M_PI/sizeof(sin_table));
		//float s = cos(2048*i*2*M_PI/sizeof(ramp_up_table));
		float s = cos(8*i*2*M_PI/sizeof(ramp_up_table));
		float c_up = cos((sizeof(ramp_up_table) - i - 1)*M_PI/sizeof(ramp_up_table)/2);
		//sin_table[i] = s * 128 + 127;
		ramp_up_table[i] = s * c_up * 128 + 127;

		//sin_table[i] = (i&1) ? 0xFF : 00;
	}

	// configure the 8 output pins, which will be mapped
	// to the DMA engine.
	GPIOD_PCOR = 0xFF;
	pinMode(2, OUTPUT);	// bit #0
	pinMode(14, OUTPUT);	// bit #1
	pinMode(7, OUTPUT);	// bit #2
	pinMode(8, OUTPUT);	// bit #3
	pinMode(6, OUTPUT);	// bit #4
	pinMode(20, OUTPUT);	// bit #5
	pinMode(21, OUTPUT);	// bit #6
	pinMode(5, OUTPUT);	// bit #7

	// enable clocks to the DMA controller and DMAMUX
	SIM_SCGC7 |= SIM_SCGC7_DMA;
	//SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

	DMA_CR = 0;

	DMA_ERQ |= DMA_ERQ_ERQ1;
	DMA_SERQ = DMA_SERQ_SERQ(1);
}


void
dma_send(
	const void * const p,
	size_t len
)
{
        DMA_TCD1_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
        DMA_TCD1_NBYTES_MLNO = len; // one byte at a time

	DMA_TCD1_SADDR = p;
        DMA_TCD1_SOFF = 1; // update byte at a time
        DMA_TCD1_SLAST = -len; // go back to the start of the buffer

	DMA_TCD1_DADDR = &GPIOD_PDOR;
        DMA_TCD1_DOFF = 0; // don't update destination
        DMA_TCD1_DLASTSGA = 0;

	// BITER sets the number of inner loops to be run;
	// must be equal to CITER when start is set.
        DMA_TCD1_CITER_ELINKNO = 1;
        DMA_TCD1_BITER_ELINKNO = 1;

        DMA_TCD1_CSR = DMA_TCD_CSR_START | (2 << 8) | (1<<5);
	//DMA_SSRT = DMA_SSRT_SSRT(1);

        DMA_TCD2_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
        DMA_TCD2_NBYTES_MLNO = len; // one byte at a time

	DMA_TCD2_SADDR = len-1 + (uint8_t*) p;
        DMA_TCD2_SOFF = -1; // update byte at a time
        DMA_TCD2_SLAST = +len; // go back to the start of the buffer

	DMA_TCD2_DADDR = &GPIOD_PDOR;
        DMA_TCD2_DOFF = 0; // don't update destination
        DMA_TCD2_DLASTSGA = 0;

	// BITER sets the number of inner loops to be run;
	// must be equal to CITER when start is set.
        DMA_TCD2_CITER_ELINKNO = 1;
        DMA_TCD2_BITER_ELINKNO = 1;

        DMA_TCD2_CSR = (1 << 8) | (1<<5);
}


static inline int
dma_complete(void)
{
	int rc = DMA_TCD1_CSR & DMA_TCD_CSR_DONE;
	//if (rc)
		//DMA_TCD1_CSR &= ~DMA_TCD_CSR_DONE;
	return rc;
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

static const uint8_t bits[] = "1234";



void
loop(void)
{
	//dma_send(sin_table, sizeof(sin_table));
	dma_send(ramp_up_table, sizeof(ramp_up_table));

	while (1)
	{
		led(0); delay(400);
		led(1); delay(200);
		led(0); delay(200);
		led(1); delay(200);
	}

	while (!dma_complete())
		;

#if 0
	uint32_t sig = 0;
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

		while (!periodic_timer_expired())
			;

#if 0
		dac_output(ps/2 + 2048);
#else
		ssb_output(ps, pc);
#endif

		sig++;
	}
#endif
}
