#pragma once

#include <Arduino.h>

// MODERN2USB controller-facing polling is kept at 1 kHz. These hooks measure the
// upstream USB host phase and make bounded, one-frame PIO scheduler corrections
// so a fresh neutral frame is ready shortly before the next upstream IN token.
void usbPollAlignmentNoteRawReport(uint8_t player, uint8_t inputClass);
void usbPollAlignmentNoteInputObserved(uint8_t player);
void usbPollAlignmentNoteSubmit(uint8_t sourcePlayer);
void usbPollAlignmentNoteUpstreamSof(uint32_t frameCount);
void usbPollAlignmentNoteUpstreamComplete(uint8_t outputSlot, bool success);
bool usbPollAlignmentUsesSof();
bool usbPollAlignmentHandleSerialCommand(const char* command, Print& out);

extern "C" void adapt_usb_input_xfer_complete(uint8_t output_slot,
                                                bool success);
