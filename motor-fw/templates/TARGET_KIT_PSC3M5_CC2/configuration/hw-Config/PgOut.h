#pragma once

#include <stdint.h>

/* PG_OUT is configured by Device Configurator on P8.5. */
#define PGOUT_PPR (8U)

void PgOut_Init(void);
void PgOut_Run(void);
void PgOut_UpdateState(void);
void PgOut_Reset(void);