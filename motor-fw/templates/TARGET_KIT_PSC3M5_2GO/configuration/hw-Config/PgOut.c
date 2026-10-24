#include "PgOut.h"
#include "cycfg_pins.h"

#if defined(PG_OUT_ENABLED)

#include "Controller.h"
#include "cy_gpio.h"
#include <math.h>

#define PGOUT_TWO_PI (6.28318530717958647692f)
#define PGOUT_LPF_TC (2048.0f)
#define PGOUT_LPF_ALPHA (1.0f / PGOUT_LPF_TC)

static float pgout_freq_filt;
static float pgout_angle;
static float pgout_delta;
static float pgout_freq_scale;

static bool PgOut_IsActive(void)
{
    STATE_ID_t state = motor[0].sm_ptr->current;

    return (state == Volt_OL) ||
           (state == Speed_CL) ||
#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_SFO)
           (state == Speed_OL_To_CL) ||
           (state == High_Freq) ||
#endif
#if defined(CTRL_METHOD_RFO)
           (state == Current_OL) ||
           (state == Current_CL) ||
           (state == Position_CL) ||
           (state == Catch_Spin) ||
#elif defined(CTRL_METHOD_SFO)
           (state == Torque_CL) ||
#elif defined(CTRL_METHOD_TBC)
           (state == Current_CL) ||
#endif
           false;
}

void PgOut_Init(void)
{
    float fs0 = motor[0].params_ptr->sys.samp.fs0;
    float pole_count = motor[0].params_ptr->motor.P;

    pgout_freq_filt = 0.0f;
    pgout_angle = 0.0f;
#if (PGOUT_PPR > 0U)
    pgout_delta = 1.0f / (2.0f * (float)PGOUT_PPR);
#else
    pgout_delta = 0.0f;
#endif
    pgout_freq_scale = 2.0f / (PGOUT_TWO_PI * fs0 * pole_count);
    Cy_GPIO_Clr(PG_OUT_PORT, PG_OUT_NUM);
}

RAMFUNC_BEGIN
void PgOut_Run(void)
{
    if (pgout_delta == 0.0f)
    {
        return;
    }

    float rotor_freq = motor[0].vars_ptr->w_final.elec * pgout_freq_scale;

    pgout_freq_filt += PGOUT_LPF_ALPHA * (rotor_freq - pgout_freq_filt);
    pgout_angle += fabsf(pgout_freq_filt);

    if (pgout_angle >= pgout_delta)
    {
        pgout_angle -= pgout_delta;
        Cy_GPIO_Inv(PG_OUT_PORT, PG_OUT_NUM);
    }
}
RAMFUNC_END

void PgOut_UpdateState(void)
{
    if (!PgOut_IsActive())
    {
        PgOut_Reset();
    }
}

void PgOut_Reset(void)
{
    pgout_freq_filt = 0.0f;
    pgout_angle = 0.0f;
    Cy_GPIO_Clr(PG_OUT_PORT, PG_OUT_NUM);
}

#endif