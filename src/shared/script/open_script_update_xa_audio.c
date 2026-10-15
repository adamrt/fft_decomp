#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/types.h"

/* The last sector of OPEN/ENDING.XA (DISC_SECTOR_OPEN_ENDING_XA is its first). */
#define ENDING_XA_LAST_SECTOR 0x3586c

/* Runs the XA audio state machine once per frame.
 *
 * Starting: sets the drive mode to 0x68 (real-time ADPCM, sub-header filter,
 * 2340-byte sectors at single speed), seeks to ENDING.XA, sets the filter to
 * file 1 and the channel in control_parameter (command 0xd, CdlSetfilter),
 * starts a streaming read and waits for it to be acknowledged; control_state
 * counts the steps and open_file_get_cd_sync_state_delta advances it past
 * each command and back one step on a disc error. A seek (step 5) still
 * unfinished after 120 frames resets the CD subsystem and starts over.
 * Playing: from two seconds in, restarts the sequence if the drive stopped
 * reading, and stops once the head (command 0x10, CdlGetlocL) has passed the
 * file's last sector.
 * Stopping: pauses the drive, restores double speed and brings the CD
 * volume back for the streams. */
void open_script_update_xa_audio(void) {
    u8 result[8];
    CdlFILTER filter;
    u8 mode;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
        switch (g_open_script_state.control_state) {
        case 0:
            mode = CdlModeRT | CdlModeSF | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 4:
            CdIntToPos(DISC_SECTOR_OPEN_ENDING_XA, &g_main_file_cd_state.position);
            CdControlF(CdlSeekL, (u8*)&g_main_file_cd_state.position);
            {
                s32* state = &g_open_script_state.control_state;

                g_main_system_session_frames = 0;
                (*state)++;
            }
            break;
        case 5: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
            if (g_main_system_session_frames >= 0x79 && *state == 5) {
                main_file_reset_cd_subsystems();
                *state = 0;
            }
        } break;
        case 6:
            filter.file = 1;
            {
                s32 parameter = g_open_script_state.control_parameter;

                filter.chan = parameter;
            }
            CdControlF(CdlSetfilter, (u8*)&filter);
            g_open_script_state.control_state++;
            break;
        case 1:
        case 7: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 8:
            CdControlF(CdlReadS, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 9:
            if (open_file_get_cd_sync_state_delta() == 1) {
                u32* control = &g_open_script_state.dispatch.flags;

                g_open_script_state.xa_wait_frames = 0;
                *control = (*control | OPEN_SCRIPT_DISPATCH_XA_PLAYING) ^ OPEN_SCRIPT_DISPATCH_XA_STARTING;
            }
            break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }

    {
        u32* control = &g_open_script_state.dispatch.flags;

        if (*control & OPEN_SCRIPT_DISPATCH_XA_PLAYING) {
            g_open_script_state.xa_wait_frames++;
            if (g_open_script_state.xa_wait_frames < 0x78) {
                return;
            }
            if (!(CdStatus() & CdlStatRead)) {
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STARTING;
                return;
            }
            if (CdControl(CdlGetlocL, 0, result) != 0 && CdPosToInt((CdlLOC*)result) > ENDING_XA_LAST_SECTOR) {
                open_sound_set_cd_mix_and_volume(0, 1);
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STOPPING;
                return;
            }
        }
    }

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
        switch (g_open_script_state.control_state) {
        case 0:
            CdControlF(CdlPause, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 2:
            mode = CdlModeSpeed;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 1:
        case 3: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 6: {
            u32* control = &g_open_script_state.dispatch.flags;

            *control ^= OPEN_SCRIPT_DISPATCH_XA_STOPPING;
            open_sound_set_cd_mix_and_volume(0xc0, 1);
        } break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }
}
