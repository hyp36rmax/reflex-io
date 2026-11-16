#pragma once

#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC

  #include <Adafruit_TinyUSB.h>
  #include "../usb_device_lock.h"

// Adafruit_USBD_CDC::begin() names the interface "TinyUSB Serial" and builds
// its descriptor in the same call, so a name set afterwards never reaches the
// host. This applies the name while the descriptor is being built.
class NamedUsbCdc : public Adafruit_USBD_CDC {
 public:
  explicit NamedUsbCdc(const char* name) : name_(name) {}
  uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t* buf, uint16_t bufsize) override {
    if (buf) setStringDescriptor(name_);
    return Adafruit_USBD_CDC::getInterfaceDescriptor(itfnum, buf, bufsize);
  }

  // Every writer goes through here, so MiSTer protocol lines can start at a
  // line boundary after output that ended without LF (tty2oled's "ttyack;").
  //
  // Adafruit_USBD_CDC::write() waits for FIFO room for as long as DTR is up.
  // A host that stops reading, for example one blocked in its own write
  // because this loop stopped reading its commands, then froze the adapter,
  // controller polling included. Write only what fits; once no room has
  // opened for kWriteStallMs, drop output until the host reads again.
  //
  // Writing claims the IN endpoint, so each chunk is written under the USB
  // lock (usb_device_lock.h), and the room is read under it too: tud_task can
  // only add room, so the chunk fits and the base write never waits. The wait
  // for room happens outside the lock, because only tud_task makes room.
  static constexpr uint32_t kWriteStallMs = 100;
  using Adafruit_USBD_CDC::write;
  size_t write(const uint8_t* buffer, size_t size) override {
    size_t written = 0;
    bool waiting = false;
    uint32_t waitStartMs = 0;
    while (written < size && dtr()) {
      int room = 0;
      size_t sent = 0;
      {
        UsbDeviceLock usbLock;
        room = availableForWrite();
        if (room > 0) {
          const size_t left = size - written;
          const size_t chunk = left < (size_t)room ? left : (size_t)room;
          sent = Adafruit_USBD_CDC::write(buffer + written, chunk);
        }
      }
      if (room > 0) {
        if (sent == 0) break;
        written += sent;
        waiting = false;
        stalled_ = false;
        continue;
      }
      if (stalled_) break;
      const uint32_t nowMs = millis();
      if (!waiting) {
        waiting = true;
        waitStartMs = nowMs;
      } else if (nowMs - waitStartMs >= kWriteStallMs) {
        stalled_ = true;
        break;
      }
      yield();  // let TinyUSB send what the host takes
    }
    if (written != 0) lastByte_ = buffer[written - 1];
    return written;
  }
  // Reading re-arms the OUT endpoint, peeking takes the receive FIFO's lock
  // (which tud_task takes on a bus reset) and flushing claims the IN endpoint.
  using Adafruit_USBD_CDC::read;
  int read(void) override {
    UsbDeviceLock usbLock;
    return Adafruit_USBD_CDC::read();
  }
  // The base function is not virtual, so call this one on a NamedUsbCdc, never
  // through an Adafruit_USBD_CDC reference.
  size_t read(uint8_t* buffer, size_t size) {
    UsbDeviceLock usbLock;
    return Adafruit_USBD_CDC::read(buffer, size);
  }
  int peek(void) override {
    UsbDeviceLock usbLock;
    return Adafruit_USBD_CDC::peek();
  }
  void flush(void) override {
    UsbDeviceLock usbLock;
    Adafruit_USBD_CDC::flush();
  }
  bool lastByteWasLineEnd() const { return lastByte_ == '\n'; }

 private:
  const char* name_;
  uint8_t lastByte_ = '\n';
  // The host took nothing for kWriteStallMs; later output is dropped at once
  // until the FIFO has room again.
  bool stalled_ = false;
};

extern NamedUsbCdc runtimeDebugCdc;
extern bool runtimeDebugCdcEnabled;

#endif
