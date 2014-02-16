!(Soft SSB being received with fldigi)[http://farm4.staticflickr.com/3732/12551247315_49f8957bd6_z_d.jpg]

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
output frequency is quite low.  Roughly 64 KHz, using 8 steps in the
carrier wave.  This has successfully transmitted a short distance to
a SSB receiver and the PSK31 signal decoded by fldigi.


PRU SSB
------------------------
The BeagleBone Black PRU is similar to the teensy design.
The userspace SDR code performs the encoding and modulation, and
writes the values to a shared buffer.  The PRU just does parallel
output to an external DAC.

This could potentially use the LCD hardware to clock out parallel
data at a high rate, although the extra capacitance from the HDMI
framer makes it unsuitable for high-speed digital IO.
