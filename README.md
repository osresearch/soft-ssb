Software SSB
============

*This is a work in progress*

Teensy SSB
----------
On the teensy 3.1, this implements a software defined radio transmitter
that encodes data as PSK31 audio in the 1 KHz range, modulates it using
SSB and then uses the builtin DAC hardware to transmit it via a wire
antenna.

Due to limitations of the DAC and the current software design, the
output frequency is quite low.  Roughly 64 KHz.


PRU SSB
------------------------
The BeagleBone Black PRU is similar to the teensy design.
The userspace SDR code performs the encoding and modulation, and
writes the values to a shared buffer.  The PRU just does parallel
output to an external DAC.

This could potentially use the LCD hardware to clock out parallel
data at a high rate, although the extra capacitance from the HDMI
framer makes it unsuitable for high-speed digital IO.
