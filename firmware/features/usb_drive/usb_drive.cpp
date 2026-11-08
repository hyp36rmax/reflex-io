#include "../../product_config.h"

#include "usb_drive.h"

#if defined(ADAPT_FEATURE_USB_DRIVE)

#include <Adafruit_TinyUSB.h>
#include <string.h>

#include "../../menu/menu_runtime_state.h"
#include "../../output/output_capabilities.h"
#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "../n64_capture/n64_capture.h"
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
#include "../host_capture/host_capture.h"
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
#include "../jogcon_capture/jogcon_capture.h"
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
#include <atomic>
#include "../psx_capture/psx_capture.h"
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
#include <atomic>
#include "../genesis_capture/genesis_capture.h"
#endif

namespace {

#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "generated/n64_capture_drive_image.inc"
#elif defined(ADAPT_FEATURE_HOST_CAPTURE)
#include "generated/host_capture_drive_image.inc"
#elif defined(ADAPT_FEATURE_JOGCON_CAPTURE)
#include "generated/jogcon_capture_drive_image.inc"
#elif defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
#include "generated/psx_capture_drive_image.inc"
#elif defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
#include "generated/genesis_capture_drive_image.inc"
#else
#include "generated/usb_drive_image.inc"
#endif

constexpr uint32_t kSectorSize = 512;
static_assert(sizeof(kUsbDriveImage) == kUsbDriveSectorCount * kSectorSize,
              "USB drive image must be whole sectors");
#if defined(ADAPT_FEATURE_N64_CAPTURE)
static_assert(kN64CaptureOverlayFileSize == n64_capture::kReportFileBytes &&
              kN64CaptureOverlaySectorCount*kSectorSize == kN64CaptureOverlayFileSize,
              "N64 FAT extent must match formatter");
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
static_assert(kHostCaptureOverlayFileSize == host_capture::kReportFileBytes &&
              kHostCaptureOverlaySectorCount * kSectorSize == kHostCaptureOverlayFileSize,
              "Host capture FAT extent must match formatter");
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
static_assert(kJogconCaptureOverlayFileSize == jogcon_capture::kReportFileBytes,
              "JogCon FAT extent must match formatter");
static_assert(kJogconCaptureOverlaySectorCount * kSectorSize == kJogconCaptureOverlayFileSize,
              "JogCon overlay must occupy whole sectors");
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
static_assert(kPsxCaptureOverlayFileSize == psx_capture::kReportFileBytes,
              "PSX capture FAT file must match the formatter contract");
static_assert(kPsxCaptureOverlaySectorCount * kSectorSize ==
                  kPsxCaptureOverlayFileSize,
              "PSX capture FAT extent must contain only whole sectors");
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
static_assert(kGenesisCaptureOverlayFileSize == genesis_capture::kReportFileBytes,
              "Genesis capture FAT file must match the formatter contract");
static_assert(kGenesisCaptureOverlaySectorCount * kSectorSize == kGenesisCaptureOverlayFileSize,
              "Genesis capture FAT extent must contain only whole sectors");
#endif

Adafruit_USBD_MSC usbDrive;

// TinyUSB hands a READ10 to the callback in endpoint-buffer chunks, and
// Adafruit's callback drops the byte offset within the sector, so a chunk
// smaller than a sector continues where the previous one stopped.
uint32_t cursorLba = 0;
uint32_t cursorOffset = 0;
bool cursorActive = false;
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
bool psxCaptureMediaRegistered = false;
std::atomic<bool> psxCaptureMediaReady{false};
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
bool genesisCaptureMediaRegistered = false;
std::atomic<bool> genesisCaptureMediaReady{false};
#endif

int32_t readImage(uint32_t lba, void* buffer, uint32_t bufsize) {
  if (lba >= kUsbDriveSectorCount) {
    return -1;
  }
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  if (!n64CaptureRecoveryActive() || !n64CapturePublished()) return -1;
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  if (!hostCaptureRecoveryActive() || !hostCapturePublished()) return -1;
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  if (!jogconCaptureRecoveryActive() || !jogconCapturePublished()) return -1;
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  if (!psxCaptureMediaReady || !psxCapturePublished()) {
    return -1;
  }
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  if (!genesisCaptureMediaReady || !genesisCapturePublished()) return -1;
#endif
  if (!cursorActive || cursorLba != lba) {
    cursorLba = lba;
    cursorOffset = 0;
    cursorActive = true;
  }
  const uint32_t length = bufsize < kSectorSize - cursorOffset ? bufsize : kSectorSize - cursorOffset;
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  if (lba >= kN64CaptureOverlayLba && lba < kN64CaptureOverlayLba+kN64CaptureOverlaySectorCount) {
    const int32_t read=n64CaptureReadFile((lba-kN64CaptureOverlayLba)*kSectorSize+cursorOffset,buffer,length);
    if(read!=static_cast<int32_t>(length)) { cursorActive=false; return -1; }
  } else
#elif defined(ADAPT_FEATURE_HOST_CAPTURE)
  if (lba >= kHostCaptureOverlayLba && lba < kHostCaptureOverlayLba + kHostCaptureOverlaySectorCount) {
    const int32_t read = hostCaptureReadFile((lba - kHostCaptureOverlayLba) * kSectorSize + cursorOffset, buffer, length);
    if (read != static_cast<int32_t>(length)) { cursorActive = false; return -1; }
  } else
#elif defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  if (lba >= kJogconCaptureOverlayLba && lba < kJogconCaptureOverlayLba + kJogconCaptureOverlaySectorCount) {
    const int32_t read = jogconCaptureReadFile((lba-kJogconCaptureOverlayLba)*kSectorSize+cursorOffset, buffer, length);
    if (read != static_cast<int32_t>(length)) { cursorActive=false; return -1; }
  } else
#elif defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  if (lba >= kPsxCaptureOverlayLba &&
      lba < kPsxCaptureOverlayLba + kPsxCaptureOverlaySectorCount) {
    const uint32_t fileOffset =
        (lba - kPsxCaptureOverlayLba) * kSectorSize + cursorOffset;
    const int32_t read = psxCaptureReadFile(fileOffset, buffer, length);
    if (read != static_cast<int32_t>(length)) {
      cursorActive = false;
      return -1;
    }
  } else
#elif defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  if (lba >= kGenesisCaptureOverlayLba &&
      lba < kGenesisCaptureOverlayLba + kGenesisCaptureOverlaySectorCount) {
    const uint32_t fileOffset = (lba - kGenesisCaptureOverlayLba) * kSectorSize + cursorOffset;
    const int32_t read = genesisCaptureReadFile(fileOffset, buffer, length);
    if (read != static_cast<int32_t>(length)) { cursorActive = false; return -1; }
  } else
#endif
  {
    memcpy(buffer, kUsbDriveImage + lba * kSectorSize + cursorOffset, length);
  }
  cursorOffset += length;
  if (cursorOffset >= kSectorSize) {
    cursorActive = false;
  }
  return static_cast<int32_t>(length);
}

int32_t refuseWrite(uint32_t, uint8_t*, uint32_t) {
  return -1;
}

void noFlush() {}

bool notWritable() {
  return false;
}

}  // namespace

void usb_drive_configure(outputMode_t effectiveMode) {
  // Console outputs remain free of MSC. The capture image registers its LUN
  // regardless of the saved drive preference, but keeps the medium unavailable
  // until the frozen report is published; this never changes the preference.
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  if (!n64CaptureRecoveryActive() || !n64CapturePublished() || !output_allows_management_cdc(effectiveMode)) return;
#elif defined(ADAPT_FEATURE_HOST_CAPTURE)
  if (!hostCaptureRecoveryActive() || !hostCapturePublished() || !output_allows_management_cdc(effectiveMode)) return;
#elif defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  if (!jogconCaptureRecoveryActive() || !jogconCapturePublished() || !output_allows_management_cdc(effectiveMode)) return;
#elif defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  if (!output_allows_management_cdc(effectiveMode)) {
    return;
  }
#else
  if (!menu_management_msc || !output_allows_management_cdc(effectiveMode)) {
    return;
  }
#endif
  cursorActive = false;
  usbDrive.setID("Reflex", PRODUCT_SHORT_NAME, "1.0");
  usbDrive.setCapacity(kUsbDriveSectorCount, kSectorSize);
  usbDrive.setReadWriteCallback(readImage, refuseWrite, noFlush);
  usbDrive.setWritableCallback(notWritable);
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  psxCaptureMediaRegistered = true;
  psxCaptureMediaReady = false;
  usbDrive.setUnitReady(false);
#elif defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  genesisCaptureMediaRegistered = true;
  genesisCaptureMediaReady = false;
  usbDrive.setUnitReady(false);
#else
  usbDrive.setUnitReady(true);
#endif
  usbDrive.begin();
}

void usb_drive_publish_psx_capture() {
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  if (!psxCaptureMediaRegistered || psxCaptureMediaReady ||
      !psxCapturePublished()) {
    return;
  }
  // The capture is immutable once published. Reset the dropped-offset cursor
  // before making the medium visible so the first READ10 starts at byte zero.
  cursorActive = false;
  cursorLba = 0;
  cursorOffset = 0;
  psxCaptureMediaReady = true;
  usbDrive.setUnitReady(true);
#endif
}

void usb_drive_publish_genesis_capture() {
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  if (!genesisCaptureMediaRegistered || genesisCaptureMediaReady || !genesisCapturePublished()) return;
  cursorActive = false; cursorLba = 0; cursorOffset = 0;
  genesisCaptureMediaReady = true;
  usbDrive.setUnitReady(true);
#endif
}

#else

void usb_drive_configure(outputMode_t effectiveMode) {
  (void)effectiveMode;
}

void usb_drive_publish_psx_capture() {}
void usb_drive_publish_genesis_capture() {}

#endif
