/*******************************************************************************
 * This file is part of PsxNewLib.                                             *
 *                                                                             *
 * Copyright (C) 2019-2020 by SukkoPera <software@sukkology.net>               *
 *                                                                             *
 * PsxNewLib is free software: you can redistribute it and/or                  *
 * modify it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * PsxNewLib is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with PsxNewLib. If not, see http://www.gnu.org/licenses.              *
 ******************************************************************************/
/**
 * \file PsxCommands.h
 * \author SukkoPera <software@sukkology.net>
 * \date 22 Mar 2021
 * \brief Playstation Controller Commands
 * 
 * Please refer to the GitHub page and wiki for any information:
 * https://github.com/SukkoPera/PsxNewLib
 */

#pragma once

#include <Arduino.h>

//! \name Controller Commands
//! @{

/** \brief Enter Configuration Mode
 * 
 * Command used to enter the controller configuration (also known as \a escape)
 * mode
 */
const byte enter_config[] = {0x01, 0x43, 0x00, 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
const byte exit_config[] = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
/* These shorter versions of enter_ and exit_config are accepted by all
 * controllers I've tested, even in analog mode, EXCEPT SCPH-1200, so let's use
 * the longer ones
 */
//~ byte enter_config[] = {0x01, 0x43, 0x00, 0x01, 0x00};
//~ byte exit_config[] = {0x01, 0x43, 0x00, 0x00, 0x00};

/** \brief Read Controller Type
 * 
 * Command used to read the controller type.
 * 
 * This does not seem to be 100% reliable, or at least we don't know how to tell
 * all the various controllers apart.
 */
const byte type_read[] = {0x01, 0x45, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
const byte c46[] = {0x01, 0x46, 0x00, /* motor index */ 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A}; //actuators
const byte set_mode[] = {0x01, 0x44, 0x00, /* enabled */ 0x01, /* locked */ 0x03, 0x00, 0x00, 0x00, 0x00};
const byte set_pressures[] = {0x01, 0x4F, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
//~ byte enable_rumble[] = {0x01, 0x4D, 0x00, 0x00, 0x01};
const byte enable_rumble[] = {0x01, 0x4D, 0x00, /* motor 1 on */ 0x00, /* motor 2 on*/ 0x01, 0xff, 0xff, 0xff, 0xff};

/** \brief Poll all buttons
 *
 * Command used to read the status of all buttons.
 */
const byte poll[] = {0x01, 0x42, 0x00, 0xFF, 0xFF};

/** \brief Poll all controllers
 *
 * Command used to read the status of all controllers when using a MultiTap.
 */
const byte multipoll[] = {0x01, 0x42, 0x01, 0x00 /* Could be 42 */, 0x00};

/** \brief Detect IR remote receiver (SCPH-10160)
 *
 * Command used to detect the PS2 IR remote receiver.
 * Uses address 0x61 (IR mode) with command 0x04.
 * Returns 0x12 in byte 1 if IR receiver is present.
 */
const byte ir_detect[] = {0x61, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
// External receiver discovery/initialization (PS2SDK rmman protocol).
const byte ir_discover[] = {0x61, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00};
const byte ir_initialize[] = {0x61, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};

// Legacy alternate address-61 command; receiver command 04 is used for IR data.
const byte ir_poll[] = {0x61, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//! @}


//! \name Controller Reply Validation
//! @{

/** \brief Check if a reply has the Digital format
 *
 * This is the earliest reply form, which includes data for 14 buttons.
 */
inline static boolean isDigitalReply (const byte *status) {
	return (status[1] & 0xF0) == 0x40;
}

/** \brief Check if a reply has the Flightstick format
 *
 * This is also called "Green Mode" because the led on SCPH-1150/1180 turns
 * green when it is enabled.
 */
inline static boolean isFlightstickReply (const byte *status) {
	return status[1] == 0x53;//return (status[1] & 0xF0) == 0x50;
}

/** \brief Check if a reply has the DualShock format
 *
 * This means it includes data for the two Analog Sticks and L3/R3.
 */
inline static boolean isDualShockReply (const byte *status) {
	return (status[1] & 0xF0) == 0x70;
}

/** \brief Check if a reply has the DualShock2 format
 *
 * This means it includes data for the two Analog Sticks, L3/R3 and analog
 * pressure levels for (almost) all buttons.
 */
inline static boolean isDualShock2Reply (const byte *status) {
	return status[1] == 0x79;
}

/** \brief Check if a reply has the Configuration Mode format
 *
 * This is only supported from DualShock onwards.
 */
inline static boolean isConfigReply (const byte *status) {
	return (status[1] & 0xF0) == 0xF0;
}

/** \brief Check if a reply has the neGcon format
 */
inline static boolean isNegconReply (const byte *status) {
	return status[1] == 0x23;
}

/** \brief Check if a reply has the JogCon format
 */
inline static boolean isJogconReply (const byte *status) {
	return status[1] == 0xE3;
}

/** \brief Check if a reply has the GunCon format
 */
inline boolean isGunconReply (const byte *status) {
	return status[1] == 0x63;
}

/** \brief Check if a reply has the Mouse format (SCPH-1030)
 */
inline boolean isMouseReply (const byte *status) {
	return status[1] == 0x12;
}

/** \brief Check if a reply has the Fishing format
 */
inline boolean isFishingReply (const byte *status) {
	return status[1] == 0xE5;
}

/** \brief Check if a reply has the Sphere 360 (spaceball) format
 */
inline boolean isSpaceballReply (const byte *status) {
	return status[1] == 0x55;
}

/** \brief Check if a reply has the MultiTap format
 *
 * This means it has DualShock-style data for 4 controllers (i.e.: 8 bytes per
 * controller).
 */
inline static boolean isMultiTapReply (const byte *status) {
	return (status[1] & 0xF0) == 0x80;
}

// PSX-SPX, Controllers - PS2 DVD Remote, command 61/04 and IR code format:
// https://psx-spx.consoledev.net/controllersandmemorycards/#controllers-ps2-dvd-remote
struct PsxIRReport { uint32_t code = 0; uint16_t pressed = 0; bool active = false; };
inline bool isPsxIRReplyHeader(const uint8_t* in, bool allowPending = false) {
  // SCPH-10160 captures contain both 00 and 5A sync on address 61. This
  // exception belongs only to native IR; ordinary PSX acceptance is unchanged.
  return in && in[0] == 0xff && (in[1] == 0x12 || (allowPending && in[1] == 0x1f)) &&
         (in[2] == 0x5a || in[2] == 0);
}
inline bool decodePsxIRReport(const uint8_t* in, PsxIRReport& report) {
  if (!isPsxIRReplyHeader(in)) return false;
  const uint32_t aligned = (uint32_t(in[4]) << 16) | (uint32_t(in[5]) << 8) | in[6];
  if (in[3] == 0) { if (aligned != 0) return false; report = {}; return true; }
  if (in[3] != 20 || (aligned & 0xf) != 0) return false;
  report = {}; report.active = true; report.code = aligned >> 4;
  if ((report.code & 0xffff0) == 0xdad50) {
    report.pressed = uint16_t(1) << (report.code & 15);
  } else if ((report.code & 0xfff00) == 0x49d00) {
    // Arrows alternate between controller and matrix IR codes while held.
    switch (report.code & 0xff) {
      case 0x79: report.pressed = uint16_t(1) << 4; break; // up
      case 0x7a: report.pressed = uint16_t(1) << 6; break; // down
      case 0x7b: report.pressed = uint16_t(1) << 7; break; // left
      case 0x7c: report.pressed = uint16_t(1) << 5; break; // right
    }
  }
  return true;
}
