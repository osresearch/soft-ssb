// \file
 /* PRU based SDR driver
 *
 */
.origin 0
.entrypoint START

#include "ws281x.hp"

/** Mappings of the GPIO devices */
#define GPIO0 (0x44E07000 + 0x100)
#define GPIO1 (0x4804c000 + 0x100)
#define GPIO2 (0x481AC000 + 0x100)
#define GPIO3 (0x481AE000 + 0x100)

/** Offsets for the clear and set registers in the devices.
 * Since the offsets can only be 0xFF, we deliberately add offsets
 */
#define GPIO_CLRDATAOUT (0x190 - 0x100)
#define GPIO_SETDATAOUT (0x194 - 0x100)


/** Register map */
#define data_addr r0
#define row r1
#define col r2
#define pin0 r3
#define gpio1_base r6
#define timer_ptr r8

#define NOP ADD r0, r0, 0

/** Wait for the cycle counter to reach a given value; we might
 * overshoot by a bit so we ensure that we always have the same
 * amount of overshoot.
 *
 * wake_time += ns;
 * while (now = read_timer()) < wake_time)
 *	;
 * if (now - wake_time < 1)
 *      nop()
 */
#define WAITNS(ns,lab) \
	MOV tmp1, (ns)/5; \
	ADD sleep_counter, sleep_counter, tmp1; \
lab: ; \
	LBBO tmp2, timer_ptr, 0xC, 4; /* read the cycle counter */ \
	QBGT lab, tmp2, sleep_counter; \
	SUB tmp2, tmp2, sleep_counter; \
	QBLT lab##_2, tmp2, 1; \
	NOP; \
lab##_2: ; \



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

    MOV timer_ptr, 0x22000 /* control register */

    // Configure our output pins
    SBBO pin0, gpio1_base, GPIO_CLRDATAOUT, 4

    MOV gpio1_base, GPIO1

    MOV pin0, 1 << 15

bit_loop:
	//SBBO pin0, gpio1_base, GPIO_CLRDATAOUT, 4
	//SBBO pin0, gpio1_base, GPIO_SETDATAOUT, 4
	CLR r30.t15
	SET r30.t15
	QBA bit_loop
	
EXIT:
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU0_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU0_ARM_INTERRUPT
#endif

    HALT
