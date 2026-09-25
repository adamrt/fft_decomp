#include "fft/event_jobstts.h"
#include "psx/types.h"

void jobstts_cmd_run_stream_with_mode(void* first, void* second, s32 event_mode) {
    jobstts_text_set_palette_and_metrics(event_mode);
    jobstts_cmd_run_stream(first, (s32)second);
}
