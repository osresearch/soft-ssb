// \file
 /* PRU based SDR driver
 *
 */
.origin 0
.entrypoint START

#include "ws281x.hp"

/** Register map */
#define zero r0
#define data_addr r1
#define offset r2
#define length r3
#define halfway r4
#define shared_ram r5

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
    // c28_pointer[15:0] field to 0x0120.  This will make C28 point to
    // 0x00012000 (PRU shared RAM).
    MOV		r0, 0x00000120
    MOV		r1, CTPPR_0
    ST32	r0, r1

    // Configure the programmable pointer register for PRU0 by setting
    // c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    // 0x80001000 (DDR memory).
    MOV		r0, 0x00100000
    MOV		r1, CTPPR_1
    ST32	r0, r1

    // Write a 0x1 into the response field so that they know we have started
    MOV r2, #0x1
    SBCO r2, CONST_PRUDRAM, 12, 4

	// ground all of our outputs
	MOV r30, 0
	MOV zero, 0

	// Clear out mailbox
	MOV shared_ram, 0x10000
	SBBO zero, shared_ram, 0, 4

restart:
	// Wait for something in the shared ram region
	LBBO offset, shared_ram, 0, 4
	QBEQ restart, offset, 0

	MOV length, 4096
	ADD length, length, offset
	MOV halfway, 300
	ADD halfway, halfway, offset

	CLR r30, 15

read_loop1:
	// Save some instructions -- read directly into the r30 output
	// each bit of output takes 25 ns;
	LBBO r30, shared_ram, offset, 2	// 15 ns?
	ADD offset, offset, 2			// 5 ns
	QBNE read_loop1, offset, halfway	// 5 ns

	// signal that we're half-way through
	SBBO zero, shared_ram, 0, 4

read_loop2:
	LBBO r30, shared_ram, offset, 2	// 15 ns?
	ADD offset, offset, 2			// 5 ns
	QBNE read_loop2, offset, length		// 5 ns

	SET r30, 15
	QBA restart
	
EXIT:
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU0_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU0_ARM_INTERRUPT
#endif

    HALT
