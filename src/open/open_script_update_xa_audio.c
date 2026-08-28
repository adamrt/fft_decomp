#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/cd.h"
#include "psx/types.h"

s32 CdStatus(void);
s32 CdPosToInt(u8* position);
void main_file_reset_cd_subsystems(void);
s32 open_file_get_cd_sync_state_delta(void);

void open_script_update_xa_audio(void) {
    u8 result[8];
    u8 play[2];
    u8 mode;

    if (g_open_script_state.dispatch.flags & 0x20) {
        switch (g_open_script_state.control_state) {
        case 0:
            mode = 0x68;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 4:
            CdIntToPos(0x2f551, g_open_file_location);
            CdControlF(CdlSeekL, g_open_file_location);
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
            if (g_main_system_session_frames >= 0x79 && *state == CdlDiskError) {
                main_file_reset_cd_subsystems();
                *state = 0;
            }
        } break;
        case 6:
            play[0] = 1;
            {
                s32 parameter = g_open_script_state.control_parameter;

                play[1] = parameter;
            }
            CdControlF(0xd, play);
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
            CdControlF(0x1b, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 9:
            if (open_file_get_cd_sync_state_delta() == 1) {
                u32* control = &g_open_script_state.dispatch.flags;

                g_open_script_state.xa_wait_frames = 0;
                *control = (*control | 8) ^ 0x20;
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

        if (*control & 8) {
            g_open_script_state.xa_wait_frames++;
            if (g_open_script_state.xa_wait_frames < 0x78) {
                return;
            }
            if (!(CdStatus() & 0x20)) {
                g_open_script_state.control_state = 0;
                *control = (*control ^ 8) | 0x20;
                return;
            }
            if (CdControl(0x10, 0, result) != 0 && CdPosToInt(result) > 0x3586c) {
                open_sound_set_type_and_volume(0, 1);
                g_open_script_state.control_state = 0;
                *control = (*control ^ 8) | 0x10;
                return;
            }
        }
    }

    if (g_open_script_state.dispatch.flags & 0x10) {
        switch (g_open_script_state.control_state) {
        case 0:
            CdControlF(9, 0);
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

            *control ^= 0x10;
            open_sound_set_type_and_volume(0xc0, 1);
        } break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }
}
