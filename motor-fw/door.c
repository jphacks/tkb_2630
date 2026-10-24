/******************************************************************************
* File Name: door.c
*
* Description: Door-resist logic on top of the motor control library running in
*              Curr_Mode_FOC_Hall. The string winds onto a spool on the shaft,
*              so opening the door turns the motor. While the door opens we
*              command torque in the winding direction, building up with the
*              distance opened after some free travel (spring) and with the
*              opening speed (damping). A hand holding the door still keeps
*              the tug going; the moment the door moves back we drop to a small
*              take-up current, so the motor never pulls the door shut. Like a
*              person, it gives in to a steady pull, so the door always opens
*              in the end. Opening at a high level starts with a short buzz,
*              and every opening is measured as the work done against the motor.
*
* The string can only pull, so the commanded current is always in the winding
* direction and never negative.
*
* Safety rules enforced here, independent of the host:
*  - drive is off until the host arms it, and again if its heartbeat stops
*  - current is capped at DOOR_I_LIMIT
*  - resisting for longer than DOOR_RESIST_MAX_MS drops to take-up (thermal)
*  - spinning fast in the winding direction (slack or broken string), or very
*    fast either way (nothing attached), cuts current
******************************************************************************/
#include <math.h>
#include "cybsp.h"
#include "Controller.h"
#include "MotorLibAPI.h"
#include "door.h"

#define DOOR_I_LIMIT            (3.0f)    /* [A] hard cap, below the DB42M03 rated 3.47 A */
#define DOOR_HB_TIMEOUT_MS      (500U)
#define DOOR_OPEN_WINDOW_MS     (120U)    /* an opening count newer than this keeps resisting */
#define DOOR_RESIST_MAX_MS      (10000U)
#define DOOR_COOLDOWN_MS        (3000U)
/* Speed limits assume the string wraps straight onto the 5 mm shaft: 1 cm of string = 15 counts */
#define DOOR_OVERSPEED_CPS      (400.0f)  /* 17 rev/s (~26 cm/s of string) while winding: string slack or broken */
#define DOOR_RUNAWAY_CPS        (1500.0f) /* 62 rev/s either way: faster than any hand pulls */
#define DOOR_RAMP_UP_A_PER_MS   (0.02f)
#define DOOR_RAMP_DOWN_A_PER_MS (0.02f)
#define DOOR_CPS_WINDOW_MS      (100U)
#define DOOR_TICK_HZ            (10000U)  /* SysTick rate: Hall sampling and the ms clock */
#define DOOR_FAULT_RETRY_MS     (2000U)   /* with an external command source the library never clears faults itself */
#define DOOR_REHOME_MS          (3000U)   /* at rest this long: take the current position as closed */
#define DOOR_KT_NM_PER_A        (0.036f)  /* torque constant, Nanotec DB42M03 datasheet */
#define DOOR_RAD_PER_COUNT      (6.2831853f / 24.0f)
#define DOOR_VEL_TAU_MS         (20.0f)   /* low-pass on the Hall-interval speed */
#define DOOR_VEL_STOP_MS        (200U)    /* no Hall edge for this long: speed is zero */
#define DOOR_BUZZ_AMP_MAX       (1.0f)    /* [A] */
#define DOOR_BUZZ_HZ_MAX        (100.0f)  /* well inside the 750 Hz current loop bandwidth */
#define DOOR_BUZZ_MAX_MS        (1000U)
#define DOOR_EVENT_REST_MS      (1500U)   /* an opening ends after this long without movement */
#define DOOR_HOLD_MAX_MS        (10000U)  /* thermal: stop pushing a door that is kept open */
#define DOOR_RESIST_STILL_MS    (1500U)   /* resisting a door held still ends after this */
#define DOOR_RELENT_PARTIAL_MS  (1500U)   /* extra wait before giving in to a tug that stalls short of full strength */

DOOR_t door =
{
    .level = 0.0f,
    .i_take = 0.3f,
    .i_resist_max = 2.0f,
    .open_dir = +1,
    .wind_sign = +1,
    .free_counts = 36,
    .ramp_counts = 24,
    .k_damp = 0.005f,
    .buzz_amp = 0.6f,
    .buzz_hz = 40.0f,
    .buzz_ms = 200U,
    .buzz_min_level = 0.5f,
    .full_counts = 72,
    .open_window_ms = 120U,
    .hold_spring = 0U,
    .close_cps_max = 60.0f,
    .relent_delay_ms = 1000U,
    .relent_ms = 2000U,
    .relent_floor = 0.0f,  /* gives in completely: the door always opens in the end */
    .recover_ms = 1500U,
    .strength = 1.0f,
    .release_counts = 3,
};

/* Hall code (u | v << 1 | w << 2) -> sector 0..5 along the DB42M03 sequence.
 * Codes 0 and 7 never occur on a healthy sensor. Swapping Hall wires only
 * reverses the order, which open_dir absorbs. */
static const int8_t hall_sector[8] = { -1, 4, 2, 3, 0, 5, 1, -1 };

/* Written in SysTick */
static int8_t sector_prev = -1;
static int32_t last_count_dir = 0;
static uint32_t last_count_ms = 0U;
static uint32_t tick_count = 0U;
static uint32_t last_count_tick = 0U;
static float inst_cps = 0.0f;   /* speed from the last Hall edge interval */
static float energy_j = 0.0f;   /* running total of work done against the motor while opening */
static uint32_t ticks_in_ms = 0U;

/* Main loop only */
static uint32_t last_hb = 0U;
static uint32_t last_hb_ms = 0U;
static uint32_t state_since_ms = 0U;
static int32_t pos_window_start = 0;
static uint32_t window_start_ms = 0U;
static uint32_t ms_prev = 0U;
static bool fault_mask_set = false;
static uint32_t last_fault_clear_ms = 0U;
static float i_base = 0.0f;     /* ramped current before the buzz is added */
static uint32_t buzz_left_ms = 0U;
static uint32_t buzz_start_ms = 0U;
static uint32_t last_buzz_req = 0U;
static uint32_t ev_start_ms = 0U;
static float ev_start_energy = 0.0f;
static float ev_peak_iq = 0.0f;
static int32_t ev_max_dist = 0;
static bool holding = false;
static uint32_t hold_start_ms = 0U;
static uint32_t engaged_ms = 0U; /* time spent resisting someone in the current opening */
static uint32_t full_ms = 0U;    /* of that, time at full spring strength */
static int32_t resist_peak = 0;  /* furthest open_dist since resisting began */

static uint32_t ReadHall(void)
{
    uint32_t u = !Cy_GPIO_Read(HALL_0_PORT, HALL_0_NUM);
    uint32_t v = !Cy_GPIO_Read(HALL_1_PORT, HALL_1_NUM);
    uint32_t w = !Cy_GPIO_Read(HALL_2_PORT, HALL_2_NUM);
    return u | (v << 1) | (w << 2);
}

static void TrackHall(uint32_t now_ms)
{
    uint32_t code = ReadHall();
    int8_t sector = hall_sector[code];
    door.hall_uvw = code;

    if (sector < 0)
    {
        door.hall_errors++;
        return;
    }
    if (sector_prev >= 0 && sector != sector_prev)
    {
        int32_t step = (sector - sector_prev + 6) % 6;
        if (step == 1 || step == 5)
        {
            int32_t dir = (step == 1) ? +1 : -1;
            uint32_t dt = tick_count - last_count_tick;
            door.pos += dir;
            last_count_dir = dir;
            last_count_ms = now_ms;
            last_count_tick = tick_count;
            inst_cps = (dt > 0U) ? (float)dir * (float)DOOR_TICK_HZ / (float)dt : 0.0f;
            if (dir == ((door.open_dir >= 0) ? +1 : -1))
            {
                /* Work done against the motor over this step: measured torque times the step angle */
                energy_j += DOOR_KT_NM_PER_A * fabsf(motor[0].vars_ptr->i_qd_r_fb.q) * DOOR_RAD_PER_COUNT;
            }
        }
        else
        {
            door.hall_errors++; /* skipped a sector: sampled too slowly or noise */
        }
    }
    sector_prev = sector;
}

static void SetState(DOOR_STATE_t state, uint32_t now_ms)
{
    if (door.state != (uint32_t)state)
    {
        door.state = (uint32_t)state;
        state_since_ms = now_ms;
    }
}

static float Clamp(float x, float lo, float hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static void StartBuzz(uint32_t now_ms)
{
    buzz_left_ms = MIN(door.buzz_ms, DOOR_BUZZ_MAX_MS);
    buzz_start_ms = now_ms;
}

/* Speed from the interval between Hall edges reacts within one edge, unlike the
 * 100 ms cps window. Between edges it can only be as fast as "one count since the
 * last edge", so it decays to zero when the shaft stops. */
static void UpdateSpeed(void)
{
    uint32_t since_ticks = tick_count - last_count_tick;
    float v = inst_cps;
    if (since_ticks >= (DOOR_TICK_HZ / 1000U) * DOOR_VEL_STOP_MS)
    {
        v = 0.0f;
    }
    else
    {
        float bound = (float)DOOR_TICK_HZ / (float)MAX(since_ticks, 1U);
        v = Clamp(v, -bound, bound);
    }
    door.vel_cps += (v - door.vel_cps) / DOOR_VEL_TAU_MS;
}

/* One opening = from entering RESIST until the door is closed again or stays still */
static void TrackOpening(uint32_t now_ms, float iq_abs)
{
    if (!door.ev_active && (door.state == DOOR_RESIST))
    {
        door.ev_active = 1U;
        ev_start_ms = now_ms;
        ev_start_energy = energy_j;
        ev_peak_iq = 0.0f;
        ev_max_dist = 0;
    }
    if (!door.ev_active)
    {
        return;
    }
    door.ev_energy_mj = (energy_j - ev_start_energy) * 1000.0f;
    ev_peak_iq = MAX(ev_peak_iq, iq_abs);
    ev_max_dist = MAX(ev_max_dist, door.open_dist);

    bool closed = (door.open_dist <= 0) && ((now_ms - ev_start_ms) > 200U);
    bool still = ((now_ms - last_count_ms) > DOOR_EVENT_REST_MS) && (door.state != DOOR_RESIST);
    if (closed || still || (door.state == DOOR_DISARMED))
    {
        door.last_energy_mj = door.ev_energy_mj;
        door.last_peak_iq = ev_peak_iq;
        door.last_max_dist = ev_max_dist;
        door.last_duration_ms = now_ms - ev_start_ms;
        door.last_gave_up = (ev_max_dist < door.full_counts) ? 1U : 0U;
        door.ev_count++;
        door.ev_active = 0U;
    }
}

static void Control(uint32_t now_ms)
{
    CTRL_VARS_t *vars_ptr = motor[0].vars_ptr;

    if (door.host_hb != last_hb)
    {
        last_hb = door.host_hb;
        last_hb_ms = now_ms;
    }
    bool host_ok = (door.armed == 1U) && ((now_ms - last_hb_ms) < DOOR_HB_TIMEOUT_MS);

    if ((now_ms - window_start_ms) >= DOOR_CPS_WINDOW_MS)
    {
        door.cps = (float)(door.pos - pos_window_start) * 1000.0f / (float)(now_ms - window_start_ms);
        pos_window_start = door.pos;
        window_start_ms = now_ms;
    }
    UpdateSpeed();

    int32_t open_dir = (door.open_dir >= 0) ? +1 : -1;
    uint32_t open_window_ms = (door.open_window_ms > 0U) ? MIN(door.open_window_ms, 2000U) : DOOR_OPEN_WINDOW_MS;
    bool opening = (last_count_dir == open_dir) && ((now_ms - last_count_ms) < open_window_ms);
    bool overspeed = ((door.cps * (float)open_dir) < -DOOR_OVERSPEED_CPS) || (fabsf(door.cps) > DOOR_RUNAWAY_CPS);

    /* Distance opened, measured from the closed door. Take-up keeps the string wound
     * while the door is shut, so the most-wound position is "closed"; after a few
     * seconds at rest, wherever the door sits counts as closed too. */
    bool at_rest = (now_ms - last_count_ms) > DOOR_REHOME_MS;
    if (((door.pos - door.home_pos) * open_dir < 0) || (door.state == DOOR_DISARMED) ||
        (at_rest && (door.state == DOOR_IDLE)))
    {
        door.home_pos = door.pos;
    }
    door.open_dist = (door.pos - door.home_pos) * open_dir;

    /* Door closer: keep the spring while the door stays open, but let go while it
     * swings shut fast (no slamming) and after DOOR_HOLD_MAX_MS (thermal) */
    bool open_past_free = door.open_dist > door.free_counts;
    if (open_past_free && !holding)
    {
        holding = true;
        hold_start_ms = now_ms;
    }
    else if (!open_past_free)
    {
        holding = false;
    }
    float v_close = MAX(-door.vel_cps * (float)open_dir, 0.0f);
    bool hold_on = (door.hold_spring != 0U) && holding && ((now_ms - hold_start_ms) < DOOR_HOLD_MAX_MS) &&
                   (v_close < door.close_cps_max);

    /* How far into the spring: 0 during free travel, 1 once fully built up */
    float build = Clamp((float)(door.open_dist - door.free_counts) / (float)MAX(door.ramp_counts, 1), 0.0f, 1.0f);

    /* Give in like a person, so the door always opens in the end: hold full strength
     * for relent_delay_ms once the spring is fully built up, then wear down to
     * relent_floor. Time adds up over the whole opening, pauses included; a tug that
     * stalls before full strength gives in too, after DOOR_RELENT_PARTIAL_MS more.
     * It only comes back once that opening is over (door closed, or left alone). */
    bool engaged = open_past_free && ((door.state == DOOR_RESIST) || hold_on);
    float relent_floor = Clamp(door.relent_floor, 0.0f, 1.0f);
    if (!door.ev_active && !engaged)
    {
        engaged_ms = 0U;
        full_ms = 0U;
        door.strength = MIN(1.0f, door.strength + 1.0f / (float)MAX(door.recover_ms, 1U));
    }
    else if (engaged)
    {
        engaged_ms++;
        if (build >= 1.0f)
        {
            full_ms++;
        }
        bool held_long_enough = (full_ms > door.relent_delay_ms) ||
                                (engaged_ms > door.relent_delay_ms + DOOR_RELENT_PARTIAL_MS);
        if (held_long_enough && (door.relent_ms > 0U))
        {
            door.strength = MAX(relent_floor, door.strength - (1.0f - relent_floor) / (float)door.relent_ms);
        }
    }

    /* Free travel first, then a spring that builds up with distance, plus damping
     * that grows with the opening speed. All scale with the level and the strength. */
    float level = Clamp(door.level, 0.0f, 1.0f) * Clamp(door.strength, 0.0f, 1.0f);
    float v_open = MAX(door.vel_cps * (float)open_dir, 0.0f);
    float i_take = Clamp(door.i_take, 0.0f, DOOR_I_LIMIT);
    float i_spring = level * build * (Clamp(door.i_resist_max, 0.0f, DOOR_I_LIMIT) - i_take);
    float i_damp = level * build * MAX(door.k_damp, 0.0f) * v_open;
    float i_resist = i_take + i_spring + i_damp;
    float i_hold = hold_on ? i_spring : 0.0f;
    float target = 0.0f;

    if (!host_ok)
    {
        SetState(DOOR_DISARMED, now_ms);
    }

    switch ((DOOR_STATE_t)door.state)
    {
    case DOOR_DISARMED:
    default:
        target = 0.0f;
        if (host_ok)
        {
            SetState(DOOR_IDLE, now_ms);
        }
        break;
    case DOOR_IDLE:
        target = i_take + i_hold;
        if (overspeed)
        {
            SetState(DOOR_COOLDOWN, now_ms);
        }
        else if (opening)
        {
            SetState(DOOR_RESIST, now_ms);
            resist_peak = door.open_dist;
            if (Clamp(door.level, 0.0f, 1.0f) >= door.buzz_min_level)
            {
                StartBuzz(now_ms); /* a warning shiver as the hand starts to open */
            }
        }
        break;
    case DOOR_RESIST:
        /* Keep resisting while the hand holds the door against us, so a steady pull is
         * a steady tug rather than catch-and-release. Stop as soon as the door has moved
         * back release_counts from its furthest point (the hand let go: never pull it
         * shut), or once it has been still a while. */
        target = MAX(i_resist, i_take);
        resist_peak = MAX(resist_peak, door.open_dist);
        if (overspeed)
        {
            SetState(DOOR_COOLDOWN, now_ms);
        }
        else if (door.open_dist <= resist_peak - MAX(door.release_counts, 1))
        {
            SetState(DOOR_IDLE, now_ms);
            i_base = MIN(i_base, i_take);
        }
        else if (!opening && ((now_ms - last_count_ms) > DOOR_RESIST_STILL_MS))
        {
            SetState(DOOR_IDLE, now_ms);
        }
        else if ((now_ms - state_since_ms) > DOOR_RESIST_MAX_MS)
        {
            SetState(DOOR_COOLDOWN, now_ms);
        }
        break;
    case DOOR_COOLDOWN:
        target = overspeed ? 0.0f : i_take;
        if (overspeed)
        {
            state_since_ms = now_ms; /* stay here until the shaft calms down */
        }
        else if ((now_ms - state_since_ms) > DOOR_COOLDOWN_MS)
        {
            SetState(DOOR_IDLE, now_ms);
        }
        break;
    }

    /* The host can buzz on its own, e.g. when tasks are left */
    if (door.buzz_req != last_buzz_req)
    {
        last_buzz_req = door.buzz_req;
        if ((door.state == DOOR_IDLE) || (door.state == DOOR_RESIST))
        {
            StartBuzz(now_ms);
        }
    }

    /* Ramp only the base current: up gently so resistance comes on smoothly,
     * down slowly so a released door eases shut */
    if (target > i_base)
    {
        i_base = MIN(target, i_base + DOOR_RAMP_UP_A_PER_MS);
    }
    else
    {
        i_base = MAX(target, i_base - DOOR_RAMP_DOWN_A_PER_MS);
    }
    if (door.state == DOOR_DISARMED)
    {
        i_base = 0.0f;
    }

    /* The buzz rides on top of the ramp (ramping it would smooth it away). The
     * string cannot push, so only the tension wobbles: base + A * (0.5 + 0.5 sin). */
    float out = i_base;
    if ((door.state == DOOR_DISARMED) || (door.state == DOOR_COOLDOWN))
    {
        buzz_left_ms = 0U;
    }
    if (buzz_left_ms > 0U)
    {
        float t = (float)(now_ms - buzz_start_ms) * 0.001f;
        float hz = Clamp(door.buzz_hz, 1.0f, DOOR_BUZZ_HZ_MAX);
        out += Clamp(door.buzz_amp, 0.0f, DOOR_BUZZ_AMP_MAX) * (0.5f + 0.5f * sinf(6.2831853f * hz * t));
        buzz_left_ms--;
    }
    door.buzz_left_ms = buzz_left_ms;
    door.i_cmd = Clamp(out, 0.0f, DOOR_I_LIMIT);

    float i_max = motor[0].params_ptr->sys.cmd.i_max;

    /* Holding torque at standstill leaves one phase near zero current for as long as
     * the rotor sits at that angle, which the phase-loss check reads as a broken wire
     * and latches a fault. Ignore it; over-current and the other checks stay active.
     * Must run after the library's first init, which installs the default masks. */
    if (!fault_mask_set && motor[0].sm_ptr->vars.init.param_init_done)
    {
        MOTOR_CTRL_SetFaultReactMask(&motor[0], No_Reaction, (FAULT_FLAGS_t){ .sw.phase_loss = 1 });
        fault_mask_set = true;
    }
    if ((door.state != DOOR_DISARMED) && (motor[0].sm_ptr->current == Fault) &&
        ((now_ms - last_fault_clear_ms) >= DOOR_FAULT_RETRY_MS))
    {
        vars_ptr->clr_faults = true;
        last_fault_clear_ms = now_ms;
    }
    float wind_sign = (door.wind_sign >= 0) ? +1.0f : -1.0f;
    vars_ptr->cmd_ext = (i_max > 0.0f) ? Clamp(wind_sign * door.i_cmd / i_max, -1.0f, 1.0f) : 0.0f;
    vars_ptr->en = (door.state != DOOR_DISARMED);

    /* What the motor actually does, to check the commanded current really flows */
    STATE_ID_t lib_state = motor[0].sm_ptr->current;
    door.iq_fb = vars_ptr->i_qd_r_fb.q;
    door.iq_ref = vars_ptr->i_qd_r_cmd.q;
    door.vdc = vars_ptr->v_dc;
    door.lib_state = (uint32_t)lib_state;
    door.lib_flags = ((lib_state == Current_CL) ? 1U : 0U) | ((lib_state == Fault) ? 2U : 0U);
    door.fault_sw = motor[0].faults_ptr->flags_latched.sw.reg;

    TrackOpening(now_ms, fabsf(door.iq_fb));
}

/* Lowest priority, so it never delays the motor control ISRs, but it still
 * preempts the main loop and samples the Hall code even if the loop blocks. */
void SysTick_Handler(void)
{
    tick_count++;
    if (++ticks_in_ms >= (DOOR_TICK_HZ / 1000U))
    {
        ticks_in_ms = 0U;
        door.fw_ms++;
    }
    TrackHall(door.fw_ms);
}

void DOOR_Init(void)
{
    /* Start disarmed: the shaft turns freely until the host takes over */
    door.state = DOOR_DISARMED;
    motor[0].vars_ptr->cmd_ext = 0.0f;
    motor[0].vars_ptr->en = false;

    (void)SysTick_Config(SystemCoreClock / DOOR_TICK_HZ);
    NVIC_SetPriority(SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
}

void DOOR_Run(void)
{
    if (door.fw_ms != ms_prev)
    {
        ms_prev = door.fw_ms;
        Control(door.fw_ms);
    }
}
