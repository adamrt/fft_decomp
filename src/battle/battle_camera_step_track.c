/* Advances one camera-track component by a frame and returns its new value.
 * Called per component by battle_camera_fusion_thread (0x8013db9c); the
 * WORLD twin is world_camera_step_track (0x800f18c8).
 *
 * Each call samples the two tangent lines of the active key segment at this
 * frame and the next, intersects them (battle_fixed_cross_product_q12 is a Q12 cross product
 * that sets g_battle_camera_cross_product_overflow on overflow) and moves the value along the resulting
 * control point. The locals span, frame and lead are reused across the
 * stages, as the target's register map requires. */
#include "fft/battle.h"
#include "psx/types.h"

typedef char battle_camera_track_offsets_ok[(sizeof(battle_camera_key_t) == 0x10
                                                && (s32) & ((battle_camera_track_t*)0)->key_count == 0x80
                                                && (s32) & ((battle_camera_track_t*)0)->finished == 0xa0)
        ? 1
        : -1];

s32 battle_camera_step_track(battle_camera_track_t* track, s32 component) {
    s32 time[4];  /* line 0: time[0]->time[1]; line 1: time[2]->time[3] */
    s32 value[4]; /* matching values for the two sampled lines */
    s32 value_delta0;
    s32 flag;
    s32 i;
    s32 frame;
    s32 span;
    s32 result;
    s32 segment;
    s32 start_value;
    s32 t0;
    s32 t1;
    s32 t2;
    s32 lead;
    s32 time_delta0;
    s32 time_delta1;
    s32 value_delta1;
    s32 divisor;
    s32 time0;
    s32 time1;
    s32 time2;
    s32 time3;
    s32 value0;
    s32 value1;
    s32 value2;
    s32 value3;

    if (track->segment_frame == 0) {
        segment = track->segment;
        t2 = track->keys[segment + 2].time;
        t1 = track->keys[segment + 1].time;
        t0 = track->keys[segment].time;
        span = t2 - t1;
        lead = t1 - t0;
        frame = t2 - t0; /* whole two-key span */
        if (track->key_count >= 4) {
            if (segment == 0) {
                track->segment_duration = span / 2 + lead;
            } else if (segment == track->key_count - 3) {
                track->segment_duration = span + lead / 2;
            } else {
                track->segment_duration = frame / 2;
            }
        } else {
            track->segment_duration = frame;
        }
        start_value = track->keys[track->segment].value;
        track->prev_value = start_value;
        if (track->key_count >= 4 && track->segment != 0) {
            track->prev_value
                = (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2 + start_value;
        }
        track->prev_value <<= 12;
        track->prev_time = track->frame << 12;
        track->start_value = track->prev_value;
    }

    if (track->keys[track->segment].value == track->keys[track->segment + 1].value
        && track->keys[track->segment].value == track->keys[track->segment + 2].value) {
        result = track->keys[track->segment].value << 12;
    } else {
        for (i = 0; i < 2; i++) {
            frame = i + track->segment_frame;
            if (track->segment == 0) {
                time[i * 2] = battle_script_interpolate_range_fixed12(track->keys[track->segment].time,
                    track->keys[track->segment + 1].time, frame, track->segment_duration);
                value[i * 2] = battle_script_interpolate_range_fixed12(track->keys[track->segment].value,
                    track->keys[track->segment + 1].value, frame, track->segment_duration);
            } else {
                time[i * 2] = battle_script_interpolate_range_fixed12(track->keys[track->segment].time,
                                  track->keys[track->segment + 1].time, frame, track->segment_duration * 2)
                    - track->keys[track->segment].time
                    + (track->keys[track->segment + 1].time - track->keys[track->segment].time) / 2;
                value[i * 2] = battle_script_interpolate_range_fixed12(track->keys[track->segment].value,
                                   track->keys[track->segment + 1].value, frame, track->segment_duration * 2)
                    - track->keys[track->segment].value
                    + (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2;
            }
            span = track->segment_duration;
            if (track->segment != track->key_count - 3) {
                span *= 2;
            }
            time[i * 2 + 1] = battle_script_interpolate_range_fixed12(
                track->keys[track->segment + 1].time, track->keys[track->segment + 2].time, frame, span);
            value[i * 2 + 1] = battle_script_interpolate_range_fixed12(
                track->keys[track->segment + 1].value, track->keys[track->segment + 2].value, frame, span);
        }
        flag = 0;
        g_battle_camera_cross_product_overflow = 0;
        value1 = value[1];
        value0 = value[0];
        time3 = time[3];
        time2 = time[2];
        time0 = time[0];
        value3 = value[3];
        value2 = value[2];
        time1 = time[1];
        value_delta0 = value1 - value0;
        time_delta1 = time3 - time2;
        value_delta1 = value3 - value2;
        time_delta0 = time1 - time0;
        divisor = battle_fixed_cross_product_q12(value_delta0, time_delta1, time_delta0, value_delta1);
        result = battle_fixed_cross_product_q12(time_delta1, value2 - value0, value_delta1, time2 - time0);
        if (divisor != 0) {
            span = battle_mul_div_s64(time_delta0, result, divisor) + time0;   /* crossing time */
            lead = battle_mul_div_s64(value_delta0, result, divisor) + value0; /* crossing value */
            divisor = span - track->prev_time;
            if (divisor != 0) {
                result = battle_mul_div_s64(lead - track->prev_value, (track->frame << 12) - track->prev_time, divisor)
                    + track->prev_value;
                track->prev_time = span;
                track->prev_value = lead;
            } else {
                flag = 1;
            }
        } else {
            flag = 1;
        }
        if (g_battle_camera_cross_product_overflow != 0 || flag != 0) {
            result = track->prev_value;
        }
    }

    track->frame++;
    if (++track->segment_frame >= track->segment_duration) {
        track->segment_frame = 0;
        if (track->key_count - 3 < ++track->segment) {
            track->finished = 1;
        }
    }
    if (component >= 3) {
        if (result > 0) {
            if ((result & 0xFFF) > 0x800) {
                result += ONE;
            }
        } else if ((result & 0xFFF) < 0x800) {
            result -= ONE;
        }
        return result / ONE;
    }
    return result / 4;
}
