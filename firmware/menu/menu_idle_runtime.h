#pragma once

#include <stdint.h>

uint32_t getScreensaverTimeoutMs();
bool isScreensaverDimMode();
bool isOledInactiveBlankMode();
// What last restarted the idle timer. Serial STATE reports it so an OLED that
// never goes idle can be traced to its cause on real hardware.
enum class IdleWakeSource : uint8_t {
  None, Other, Controller, Buttons, Menu, Serial, HostRumble, AutoDetect, MisterArtwork,
};
// Restore a previously woken JVS OLED while no response is pending.
void serviceDeferredIdleDisplayRestore();
void resetIdleTimer(IdleWakeSource source = IdleWakeSource::Other);
bool handleIdleUiActivity(bool uiActivity,
                          IdleWakeSource source = IdleWakeSource::Menu);
IdleWakeSource lastIdleWakeSource();
const char* idleWakeSourceName(IdleWakeSource source);
bool handleControllerIdleWakeActivity();
// OLED Wake = Adapt only: controller input, connections and host rumble
// never wake the panel or restart its timer outside a menu.
bool isOledWakeAdaptOnly();
bool isIdleTimeoutReached();
void resetAnimationState();
