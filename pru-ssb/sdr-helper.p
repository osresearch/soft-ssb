// \file
 /* PRU1 helps PRU0 by copying from DDR to SHAREDRAM.
 * This avoids latency issues with reading from DDR (100-200ns),
 * while allowing the user space application to take advantage of
 * this faster access to the DDR.
 *
 */
.origin 0
.entrypoint START

#include "ws281x.hp"

/** Register map */
#define zero r0
#define data_addr r1
#define offset r2
#define out_offset r3
#define length r4
#define shared_ram r5
#define frame r6


#define NOP ADD r0, r0, 0


START:
    // Enable OCP master port
    // clear the STANDBY_INIT bit in the SYSCFG register,
    // otherwise the PRU will not be able to write outside the
    // PRU memory space and to the BeagleBon's pins.
    LBCO	r0, C4, 4, 4
    CLR		r0, r0, 4
    SBCO	r0, C4, 4, 4

    // Configure the programmable pointer register for PRU0 by setting
    // c28_pointer[15:0] field to 0x0100.  This will make C28 point to
    // 0x00010000 (PRU shared RAM).
    MOV		r0, 0x00000120
    MOV		r1, CTPPR_0
    ST32	r0, r1

    // Configure the programmable pointer register for PRU0 by setting
    // c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    // 0x80001000 (DDR memory).
    MOV		r0, 0x00100000
    MOV		r1, CTPPR_1
    ST32	r0, r1

	MOV zero, 0
	MOV frame, 0
	MOV length, 4096
	MOV shared_ram, 0x10000
	SBBO zero, shared_ram, 0, 4

restart:
	LBCO data_addr, CONST_PRUDRAM, 0, 4
	QBEQ restart, data_addr, 0

wait_loop:
	// Wait for the other PRU to indicate that it is halfway
	LBBO offset, shared_ram, 0, 4
	QBNE wait_loop, offset, 0

	// Let the user know we have read the command message
	// and are making progress on it
	SBCO zero, CONST_PRUDRAM, 0, 4

	// alternate between two different buffers
	MOV out_offset, 8192
	XOR frame, frame, 1
	QBEQ other_frame, frame, 0
	MOV out_offset, 1024
other_frame:

	MOV offset, 0

read_loop:
	// Read 16 registers, 64 bytes worth of data
	LBBO r10, data_addr, offset, 4*16
	SBBO r10, shared_ram, out_offset, 4*16
	ADD offset, offset, 4*16
	ADD out_offset, out_offset, 4*16
	QBNE read_loop, offset, length

	// Write the command to the other PRU
	// should give it the offset where we started
	SUB out_offset, out_offset, length
	SBBO out_offset, shared_ram, 0, 4

	// Wait for a new command
	QBA restart

EXIT:
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU0_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU0_ARM_INTERRUPT
#endif

    HALT
