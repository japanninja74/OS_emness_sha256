/**
* File              : gv_sha256.h
*                     (Hardware Abstraction Layer)
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
* Last Modified Date: 05.07.2023
*
 * Copyright (c) 2023
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef GV_SHA256_H
#define GV_SHA256_H

#include "xil_hal.h"
#include "xparameters.h"

#define _IO volatile
#define _O  volatile const

typedef struct {
  _IO u32 CSR; /* 31    30      29        XXX     10    9   8     7:4      3:2       1         0      */
               /* ERR  DONE   BLOCK_READ        IENABLE WR LAST LASTWORD VALIDBYTE NEWHASH COREENABLE */
                     
  _IO u32 W0;  /* most significant message block word */
  _IO u32 W1;
  _IO u32 W2;
  _IO u32 W3;
  _IO u32 W4;
  _IO u32 W5;
  _IO u32 W6;
  _IO u32 W7;
  _IO u32 W8;
  _IO u32 W9;
  _IO u32 W10;
  _IO u32 W11;
  _IO u32 W12;
  _IO u32 W13;
  _IO u32 W14;
  _IO u32 W15;

  _O  u32 H7;  /* most significant hash word */
  _O  u32 H6;
  _O  u32 H5;
  _O  u32 H4;
  _O  u32 H3;
  _O  u32 H2;
  _O  u32 H1;
  _O  u32 H0;
} GV_SHA256_TypeDef;


/* Access a memory mapped GV_SHA256 register */
#define GV_SHA256 ((GV_SHA256_TypeDef*) XPAR_GV_SHA256_0_S00_AXI_BASEADDR)

#endif // GV_SHA256_H

