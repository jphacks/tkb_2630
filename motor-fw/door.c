/******************************************************************************
* File Name: door.c
*
* Description: Door-resist logic on top of the motor control library running in
*              Curr_Mode_FOC_Hall. The string winds onto a spool on the shaft,
*              so opening the door turns the motor. While the door opens we
*              command torque in the winding direction; the moment it stops or
*              closes we drop back to a small take-up current so the door never
*              slams shut on a hand.
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
#define DOOR_RAMP_UP_A_PER_MS   (0.10f)
#define DOOR_RAMP_DOWN_A_PER_MS (0.02f)
#define DOOR_CPS_WINDOW_MS      (100U)
#define DOOR_TICK_HZ            (10000U)  /* SysTick rate: Hall sampling and the ms clock */
#define DOOR_FAULT_RETRY_MS     (2000U)   /* with an external command source the library never clears faults itself */

DOOR_t door =
{
    .level = 0.0f,
    .i_take = 0.3f,
    .i_resist_max = 2.0f,
    .open_dir = +1,
    .wind_sign = +1,
};

/* Hall code (u | v << 1 | w << 2) -> sector 0..5 along the DB42M03 sequence.
 * Codes 0 and 7 never occur on a healthy sensor. Swapping Hall wires only
 * reverses the order, which open_dir absorbs. */
static const int8_t hall_sector[8] = { -1, 4, 2, 3, 0, 5, 1, -1 };

static int8_t sector_prev = -1;
static int32_t last_count_dir = 0;
static uint32_t last_count_ms = 0U;
static uint32_t last_hb = 0U;
static uint32_t last_hb_ms = 0U;
static uint32_t state_since_ms = 0U;
static int32_t pos_window_start = 0;
static uint32_t window_start_ms = 0U;
static uint32_t ms_prev = 0U;
static uint32_t ticks_in_ms = 0U;
static bool fault_mask_set = false;
static uint32_t last_fault_clear_ms = 0U;

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
            door.pos += dir;
            last_count_dir = dir;
            last_count_ms = now_ms;
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

static void Control(uint32_t now_ms)
{
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

    int32_t open_dir = (door.open_dir >= 0) ? +1 : -1;
    bool opening = (last_count_dir == open_dir) && ((now_ms - last_count_ms) < DOOR_OPEN_WINDOW_MS);
    bool overspeed = ((door.cps * (float)open_dir) < -DOOR_OVERSPEED_CPS) || (fabsf(door.cps) > DOOR_RUNAWAY_CPS);

    float i_take = Clamp(door.i_take, 0.0f, DOOR_I_LIMIT);
    float i_resist = i_take + Clamp(door.level, 0.0f, 1.0f) * (Clamp(door.i_resist_max, 0.0f, DOOR_I_LIMIT) - i_take);
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
        target = i_take;
        if (overspeed)
        {
            SetState(DOOR_COOLDOWN, now_ms);
        }
        else if (opening)
        {
            SetState(DOOR_RESIST, now_ms);
        }
        break;
    case DOOR_RESIST:
        target = MAX(i_resist, i_take);
        if (overspeed)
        {
            SetState(DOOR_COOLDOWN, now_ms);
        }
        else if (!opening)
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

    /* Ramp up fast so resistance is felt at once, down slowly so a released door eases shut */
    if (target > door.i_cmd)
    {
        door.i_cmd = MIN(target, door.i_cmd + DOOR_RAMP_UP_A_PER_MS);
    }
    else
    {
        door.i_cmd = MAX(target, door.i_cmd - DOOR_RAMP_DOWN_A_PER_MS);
    }
    if (door.state == DOOR_DISARMED)
    {
        door.i_cmd = 0.0f;
    }

    CTRL_VARS_t *vars_ptr = motor[0].vars_ptr;
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
}

/* Lowest priority, so it never delays the motor control ISRs, but it still
 * preempts the main loop and samples the Hall code even if the loop blocks. */
void SysTick_Handler(void)
{
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
