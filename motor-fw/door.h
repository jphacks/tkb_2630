/******************************************************************************
* File Name: door.h
*
* Description: Resists opening a door that is tied to the motor shaft with a
*              string. The host (Mac) sets how hard to resist over J-Link by
*              writing into the `door` struct, and reads telemetry from it.
*
* Every field is 4 bytes so the host can map the struct with one fixed format.
* Keep the field order in sync with host/door_link.py.
******************************************************************************/
#ifndef DOOR_H
#define DOOR_H

#include <stdint.h>

typedef enum
{
    DOOR_DISARMED = 0, /* drive off, shaft turns freely */
    DOOR_IDLE,         /* small take-up current keeps the string taut */
    DOOR_RESIST,       /* door is being opened: resist with `level` */
    DOOR_COOLDOWN,     /* resisted too long or string went slack: take-up only */
} DOOR_STATE_t;

typedef struct
{
    /* Written by the host */
    volatile uint32_t host_hb;      /* host increments this; drive is cut when it stops changing */
    volatile uint32_t armed;        /* 1 = allow driving the motor */
    volatile float level;           /* 0..1, how hard to resist opening */
    volatile float i_take;          /* [A] take-up current that keeps the string taut */
    volatile float i_resist_max;    /* [A] current at level = 1 */
    volatile int32_t open_dir;      /* +1/-1: Hall count direction that means "door opening" */
    volatile int32_t wind_sign;     /* +1/-1: sign of the current that winds the string back */

    /* Telemetry, read by the host */
    volatile uint32_t fw_ms;        /* milliseconds since boot */
    volatile int32_t pos;           /* Hall counts, 24 per mechanical revolution */
    volatile float cps;             /* counts per second, signed, 100 ms window */
    volatile uint32_t hall_uvw;     /* raw Hall code, u | v << 1 | w << 2 */
    volatile uint32_t hall_errors;  /* invalid codes or skipped sectors */
    volatile uint32_t state;        /* DOOR_STATE_t */
    volatile float i_cmd;           /* [A] current being commanded */
    volatile float iq_fb;           /* [A] measured torque-producing current, signed */
    volatile float iq_ref;          /* [A] the library's own q-axis command after its limits */
    volatile float vdc;             /* [V] DC bus voltage */
    volatile uint32_t lib_state;    /* raw STATE_ID_t of the motor control library */
    volatile uint32_t lib_flags;    /* bit0: in Current_CL, bit1: in Fault */
    volatile uint32_t fault_sw;     /* latched software fault bits of the library (FAULT_FLAGS_t.sw) */

    /* Written by the host: how far the door opens before resistance builds up */
    volatile int32_t free_counts;   /* opening distance with take-up only */
    volatile int32_t ramp_counts;   /* distance over which resistance grows to `level` */

    /* Telemetry */
    volatile int32_t home_pos;      /* pos of the closed door: the most-wound position seen at rest */
    volatile int32_t open_dist;     /* counts opened past home */

    /* Written by the host: feel of the door */
    volatile float k_damp;          /* [A per count/s] viscous resistance while opening */
    volatile float buzz_amp;        /* [A] tension wobble added for the warning buzz (capped at 1 A) */
    volatile float buzz_hz;         /* buzz frequency */
    volatile uint32_t buzz_ms;      /* buzz length */
    volatile uint32_t buzz_req;     /* host increments this to buzz now */
    volatile float buzz_min_level;  /* opening at this level or above starts with a buzz */
    volatile int32_t full_counts;   /* opening this far counts as "got it"; less is "gave up" */

    /* Telemetry: speed and the energy spent opening ("how much you wanted it") */
    volatile float vel_cps;         /* counts per second from Hall edge intervals, low-passed, signed */
    volatile uint32_t buzz_left_ms;
    volatile uint32_t ev_active;    /* 1 while an opening is being measured */
    volatile uint32_t ev_count;     /* number of finished openings */
    volatile float ev_energy_mj;    /* [mJ] work done against the motor in the current opening */
    volatile float last_energy_mj;  /* [mJ] of the last finished opening */
    volatile float last_peak_iq;    /* [A] peak measured current in that opening */
    volatile int32_t last_max_dist; /* counts: how far that opening got */
    volatile uint32_t last_duration_ms;
    volatile uint32_t last_gave_up; /* 1 if it closed again before reaching full_counts */

    /* Written by the host: mounting. A string can only pull while the door opens;
     * a shaft fixed to the door at the hinge sees only 24 counts per turn
     * (6 for a 90 degree swing) and can also push the door shut. */
    volatile uint32_t open_window_ms; /* an opening count newer than this keeps resisting */
    volatile uint32_t hold_spring;    /* 1: keep the spring force while the door is held open (door closer) */
    volatile float close_cps_max;     /* swinging shut faster than this: stop resisting or holding at once */

    /* Written by the host: give in like a person. After resisting a steady pull for
     * relent_delay_ms, the strength eases off to relent_floor over relent_ms, and
     * comes back over recover_ms once nobody is pushing against it. */
    volatile uint32_t relent_delay_ms;
    volatile uint32_t relent_ms;      /* 0: never give in */
    volatile float relent_floor;
    volatile uint32_t recover_ms;

    /* Telemetry */
    volatile float strength;          /* 0..1 multiplier on the spring, damping and hold */

    /* Written by the host: how far the door must move back from its furthest point
     * while resisting before we treat it as let go. A single count would chatter
     * whenever the motor slightly outpulls the hand. */
    volatile int32_t release_counts;

    /* Written by the host: how long the shaft must sit short of the closed point before
     * the wind-back pulls harder. Too short and a lid let go while open is pulled shut
     * at once; 0 = DOOR_WIND_STILL_MS. */
    volatile uint32_t wind_wait_ms;
} DOOR_t;

extern DOOR_t door;

void DOOR_Init(void);
void DOOR_Run(void); /* call from the main loop as often as possible */

#endif /* DOOR_H */
