#include "platform_priority_output_runtime.h"

#include "../../core/runtime/runtime_deferred_services.h"
#include "../../core/serial_neutral_frame_injection.h"
#include "../../output/runtime/output_loop_runtime.h"

bool runPlatformPriorityOutputOverride() {
  if (!serialNeutralFrameInjectionOwnsPriorityOutput()) {
    return false;
  }
  serialNeutralFrameInjectionService();
  serialNeutralFrameInjectionPrepareOutputFrame();
  sendPreparedOutputFrame();
  serialNeutralFrameInjectionNoteOutputStaged();
  serialNeutralFrameInjectionRestoreOutputFrame();
  runDeferredRuntimeServices();
  return true;
}

void servicePlatformPriorityOutput() {
  serialNeutralFrameInjectionService();
}

void preparePlatformPriorityOutputFrame() {
  serialNeutralFrameInjectionPrepareOutputFrame();
}

void finishPlatformPriorityOutputFrame() {
  serialNeutralFrameInjectionNoteOutputStaged();
  serialNeutralFrameInjectionRestoreOutputFrame();
}
