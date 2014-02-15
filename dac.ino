float phase = 0.0;
float twopi = 3.14159 * 2;
elapsedMicros usec = 0;

uint16_t sin_table[128];

void setup() {
  analogWriteResolution(12);
  analogWrite(A14, 0);

  for (int i = 0 ; i < 128 ; i++)
	sin_table[i] = (1+sin(i * M_PI / 64)) * 512;
}



void loop() {
  //float val = sin(phase) * 2000.0 + 2050.0;
  unsigned i = 0;
  uint32_t sig = 0;
  while(1)
  {
     *(volatile uint16_t*)&(DAC0_DAT0L) = (sin_table[i] * ((sig / 65536) % 512)) / 512;
     i = (i + 16) % 128;
     sig++;
  }
 // phase = phase + 0.02;
 // if (phase >= twopi) phase = 0;
  //while (usec < 500) ; // wait
  //usec = usec - 500;
}
