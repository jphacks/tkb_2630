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
} DOOR_t;

extern DOOR_t door;

void DOOR_Init(void);
void DOOR_Run(void); /* call from the main loop as often as possible */

#endif /* DOOR_H */
