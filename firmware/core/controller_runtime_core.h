#pragma once

// Internal boundary for the controller translation pipeline that runs between
// raw input polling and output transport send.

void restoreControllerFramesBeforeInputPoll();
void processPolledInputFrame(bool updated);
void cacheProcessedControllerOutputState();
bool processedControllerFrameNeedsFinalize();
void finalizeControllerFrameForOutput();
void restoreControllerFrameAfterOutputSend();
