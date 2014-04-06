/** \file
 * Software SSB transmitter for the Teensy 3.1 using the DMA engine
 * to drive an external DAC connected to PORTD.
 *
 * Good reference to the DMA engine: http://forum.pjrc.com/threads/23950-Parallel-GPIO-on-Teensy-3-0
 * GPIO mappings on the Teensy 3.1: http://forum.pjrc.com/threads/17532-Tutorial-on-digital-I-O-ATMega-PIN-PORT-DDR-D-B-registers-vs-ARM-GPIO_PDIR-_PDOR
 *
 * The DMA clocks out the bytes to the GPIO at 20 MHz.
 * This limits the possible output speed to about 1 MHz.
 *
 * The ramp up / ramp down for the PSK31 signal is 15 ms, which would require
 * too much memory to store (294 KB).  Instead 64 512 byte snippets of
 * carrier are used, each at a different amplitude.
 *
 * The DMA engines ping-pongs between buffer 1 and buffer 2.
 */

#define POWER_LEVELS	32
#define DMA_LENGTH	512
#define FREQUENCY	32 // multiple of 40 KHz
uint8_t carrier[2][POWER_LEVELS][DMA_LENGTH];

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

	for (int power = 0 ; power < POWER_LEVELS ; power++)
	{
		float p = sin(power * M_PI/POWER_LEVELS/2);

		for (int t = 0 ; t < DMA_LENGTH ; t++)
		{
			float c1 = sin(FREQUENCY * t * 2 * M_PI / DMA_LENGTH);
			float c2 = cos(FREQUENCY * t * 2 * M_PI / DMA_LENGTH);
			//float c = 1; // to display just the phase
			carrier[0][power][t] = c1 * p * 128 + 127;
			carrier[1][power][t] = c2 * p * 128 + 127;
		}
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

	DMA_ERQ |= DMA_ERQ_ERQ1 | DMA_ERQ_ERQ2;

	// operate byte at a time on reads and writes
        DMA_TCD1_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
        DMA_TCD2_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);

	// configure both for a no power state
	DMA_TCD1_SADDR = carrier[0][0];
	DMA_TCD2_SADDR = carrier[0][0];

        DMA_TCD1_NBYTES_MLNO = DMA_LENGTH;
        DMA_TCD2_NBYTES_MLNO = DMA_LENGTH;

	// update byte at a time
        DMA_TCD1_SOFF = 1;
        DMA_TCD2_SOFF = 1;

	// go back to the start of the buffer after the major transfer
        DMA_TCD1_SLAST = -DMA_LENGTH;
        DMA_TCD2_SLAST = -DMA_LENGTH;

	// write the output to the PORTD
	DMA_TCD1_DADDR = &GPIOD_PDOR;
	DMA_TCD2_DADDR = &GPIOD_PDOR;

	// don't update destination at all; each byte writes to GPIOD_PDOR
        DMA_TCD1_DOFF = 0;
        DMA_TCD2_DOFF = 0;

        DMA_TCD1_DLASTSGA = 0;
        DMA_TCD2_DLASTSGA = 0;

	// BITER sets the number of inner loops to be run;
	// must be equal to CITER when start is set.
        DMA_TCD1_CITER_ELINKNO = 1;
        DMA_TCD1_BITER_ELINKNO = 1;

        DMA_TCD2_CITER_ELINKNO = 1;
        DMA_TCD2_BITER_ELINKNO = 1;

	// DMA1 links to DMA2 which links to DMA1
        DMA_TCD1_CSR = DMA_TCD_CSR_MAJORLINKCH(2) | DMA_TCD_CSR_MAJORELINK;
        DMA_TCD2_CSR = DMA_TCD_CSR_MAJORLINKCH(1) | DMA_TCD_CSR_MAJORELINK;

	// start DMA1
	DMA_TCD1_CSR |= DMA_TCD_CSR_START;
}


#if 0
/** Configure DMA1 to chain to DMA2, which chains to DMA1. */
void
dma_send(
	const void * const p1,
	const void * const p2,
	size_t len
)
{
        DMA_TCD1_ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
        DMA_TCD1_NBYTES_MLNO = len; // one byte at a time

	DMA_TCD1_SADDR = p1;
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

	DMA_TCD2_SADDR = p2;
        DMA_TCD2_SOFF = 1; // update byte at a time
        DMA_TCD2_SLAST = -len; // go back to the start of the buffer

	DMA_TCD2_DADDR = &GPIOD_PDOR;
        DMA_TCD2_DOFF = 0; // don't update destination
        DMA_TCD2_DLASTSGA = 0;

	// BITER sets the number of inner loops to be run;
	// must be equal to CITER when start is set.
        DMA_TCD2_CITER_ELINKNO = 1;
        DMA_TCD2_BITER_ELINKNO = 1;

        DMA_TCD2_CSR = (1 << 8) | (1<<5);
}
#endif


/**
 * Swap the DMA engines safely.
 * Wait for dma1 to be "done" so that we can swap it safely.
 * then wait for dma2 to be "done".  
 */
static void
dma_swap(
	const void * const buf
)
{
	// clear the DMA1 done flag and wait for it to be reset
	DMA_CDNE = DMA_CDNE_CDNE(1);
	while (!(DMA_TCD1_CSR & DMA_TCD_CSR_DONE))
		;
	// DMA1 is now safe to swap
	DMA_TCD1_SADDR = buf;

	// clear the DMA2 done flag and wait for it to be reset
	DMA_CDNE = DMA_CDNE_CDNE(2);
	while (!(DMA_TCD2_CSR & DMA_TCD_CSR_DONE))
		;
	// DMA2 is now safe to swap
	DMA_TCD2_SADDR = buf;
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



static void
send(int bit)
{
	static int phase;

	if (bit == 0)
	{
		// ramp down
		for (int power = POWER_LEVELS-1 ; power >= 0 ; power--)
		{
			dma_swap(carrier[phase][power]);
			delayMicroseconds(500);
		}

		// and ramp back up on the other phase
		phase = !phase;

		for (int power = 0 ; power < POWER_LEVELS ; power++)
		{
			dma_swap(carrier[phase][power]);
			delayMicroseconds(500);
		}
	} else {
		for (int power = 0 ; power < 2*POWER_LEVELS ; power++)
		{
			dma_swap(carrier[phase][POWER_LEVELS-1]);
			delayMicroseconds(500);
		}
	}
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


void
loop(void)
{
	//dma_send(sin_table, sizeof(sin_table));
	//dma_send(ramp_up_table, sizeof(ramp_up_table));

	for (int i = 0 ; i < sizeof(psk_bits) ; i++)
	{
		send(psk_bits[i] == '1');
	}

#if 0
	while (1)
	{
		led(0); delay(400);
		led(1); delay(200);
		led(0); delay(200);
		led(1); delay(200);
	}

	while (!dma_complete())
		;

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
