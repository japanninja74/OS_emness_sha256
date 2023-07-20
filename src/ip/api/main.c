/**
* File              : main.c
* 					  gv_sha256 self test program
*
* Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
*                     Isacco Delpero      <s314713@studenti.polito.it>
*                     Leonardo Cerruti    <s317664@studenti.polito.it>
*                     Claudio Capobianchi <s319187@studenti.polito.it>
*                     Saman Alipour       <s307980@studenti.polito.it>
*                     Abdul Rehman        <s315198@studenti.polito.it>
*                     Stephano Perera     <s313080@studenti.polito.it>
*
* Date              : 04.07.2023
* Last Modified Date: 20.07.2023
*
* Copyright (c) 2023
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xpseudo_asm_gcc.h"	/* cortexa9 inline assembly */
#include "xparameters.h"        /* address definitions */
#include "xparameters_ps.h"     /* cortexa9 core peripherals address definitions */
#include "xil_hal.h"            /* collection of hal headers */
#include "xil_exception.h"      /* exception handlers hal */

#include "gv_sha256.h"			/* cryptocore HAL */
#include "xscugic.h"    		/* cortexa9 GIC HAL */

#include "xil_printf.h"

int gicSetup();
void checkHash();

void sha256Handler(void*);

static volatile int block;

int main() {

	xil_printf("################################\n\r");
	xil_printf("# gv_sha256 self test\n\r");
	xil_printf("# NIST-ADDITIONAL-SHA256: n. 7\n\r");
	xil_printf("################################\n\r\n\r");

	xil_printf("Starting polling test...\n\r");

    /* core_enable, new_hash, 16 words, 4 bytes */
	GV_SHA256->CSR = 0xf3;

	/* transfer test block */
	for(int i = 0; i < 16; i++)
		*(((u32*)&(GV_SHA256->W0))+i) = 0;

	/* process the same zeroed block repeatedly */
	for(int block = 0; block < 15624; block++) {

		/* start */
		GV_SHA256->CSR |= 0x1 << 9;

		/* wait for completion */
		while (!(GV_SHA256->CSR & (0x1 << 29)));
	}

	/* last message block, 16 words, 4 bytes, start */
	GV_SHA256->CSR = 0x3f1;

	/* wait for hash computation to finish */
	while (!(GV_SHA256->CSR & (0x1 << 30)));
	checkHash();

	xil_printf("\n\r################################\n\r\n\r");
	xil_printf("Starting interrupt test...\n\r");

	/* core_enable, new_hash, 16 words, 4 bytes */
	GV_SHA256->CSR = 0x4f3;

	if (gicSetup())
	  xil_printf("Configuration failed...\n\r");
	else {
	  xil_printf("Configuration done...\n\r");

	  /* process block */
	  block = 0;
	  GV_SHA256->CSR |= 0x1 << 9;
	}
	
	while(1);
}

int gicSetup() {
	XScuGic gic;
	XScuGic_Config *gic_config;

	/* mfcpsr(): 0x600000DF
	 * little endian
	 * 'system' processor mode
	 * asynchronous abort
	 * IRQ masked
	 * FIQ masked
	 */

	if (!(gic_config = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID))) {
		xil_printf("LookupConfig() failed\n\r");
		return -1;
	}

	if (XScuGic_CfgInitialize(&gic, gic_config, gic_config->CpuBaseAddress) != XST_SUCCESS) {
		xil_printf("CfgInitialize() failed\n\r");
		return -1;
	}

	/*
	 * Register the handler for the exception 'IRQ interrupt'.
	 *
	 * XScuGic_InterruptHandler: primary interrupt handler for the driver.
	 * It resolves which interrupts are active and enabled and calls the
	 * appropriate interrupt handler
	 *
	 * WATCH OUT! Customization may be needed here: xscugic_intr.c
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler, &gic);

	/* Binds the id of the interrupt source to its handler.
	 *   - XPS_FPGA0_INT_ID : done & block_read
	 */
	if (XScuGic_Connect(&gic, XPS_FPGA0_INT_ID,
			(Xil_ExceptionHandler) sha256Handler, (void*) &gic) != XST_SUCCESS ) {
		xil_printf("XScuGic_Connect() failed\n\r");
		return -1;
	}

	/* Configure interrupt detection in the GIC (distributor) */
	XScuGic_WriteReg(XPAR_PS7_SCUGIC_0_DIST_BASEADDR,
			XSCUGIC_INT_CFG_OFFSET+12, 	/* ICDICFR3 */
			XScuGic_ReadReg(XPAR_PS7_SCUGIC_0_DIST_BASEADDR, XSCUGIC_INT_CFG_OFFSET+12)
			| 0x3 << 26);	/* rising edge sensitivity */
	if (((XScuGic_ReadReg(XPAR_PS7_SCUGIC_0_DIST_BASEADDR, XSCUGIC_INT_CFG_OFFSET+12) >> 26) & 0x3) != 0x3) {
		xil_printf("XScuGic: unable to configure sensitivity\n\r");
		return -1;
	}

	/* Enable interrupts in the GIC */
	XScuGic_Enable(&gic, XPS_FPGA0_INT_ID);

	/* Enable exceptions in the cortex9 */
	Xil_ExceptionEnable();

	/* mfcpsr(): 0x600F005F
	 * little endian
	 * 'system' processor mode
	 * asynchronous abort
	 * >> IRQ unmasked <<
	 * FIQ masked
	 */
	if(mfcpsr() & 1 << 7) {
		xil_printf("Xil_ExceptionEnable() failed\n\r");
		return -1;
	}

	return 0;
}

void sha256Handler(void*) {
	/* determine source */
	if (GV_SHA256->CSR & (0x1 << 29)) {
		
		/* block read */
		if (++block < 15624)
			GV_SHA256->CSR |= 0x1 << 9; /* write */
		else
			GV_SHA256->CSR = 0x7f1; /* last message block, 16 words, 4 bytes, start */

	} else if (GV_SHA256->CSR & (0x1 << 30)) {
		
		/* done */
		checkHash();
	
	} else
		xil_printf("sha256Handler(): uncaught\n\r");
}

void checkHash() {
	xil_printf("Hash: %08X_%08X_%08X_%08X_%08X_%08X_%08X_%08X\n\r",
			GV_SHA256->H7, GV_SHA256->H6, GV_SHA256->H5,
			GV_SHA256->H4, GV_SHA256->H3, GV_SHA256->H2,
			GV_SHA256->H1, GV_SHA256->H0);

	if ((GV_SHA256->H7 != 0xd29751f2) |
		(GV_SHA256->H6 != 0x649b32ff) |
		(GV_SHA256->H5 != 0x572b5e0a) |
		(GV_SHA256->H4 != 0x9f541ea6) |
		(GV_SHA256->H3 != 0x60a50f94) |
		(GV_SHA256->H2 != 0xff0beedf) |
		(GV_SHA256->H1 != 0xb0b692b9) |
		(GV_SHA256->H0 != 0x24cc8025) ) {
		xil_printf("Test failed...\n\r");
	} else
		xil_printf("Test passed...\n\r");
}







