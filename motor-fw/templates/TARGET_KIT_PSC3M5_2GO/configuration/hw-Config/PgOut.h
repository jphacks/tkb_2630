#pragma once

#include <stdint.h>

/* PG_OUT is configured by Device Configurator on P7.1 for the 2GO board. */
#define PGOUT_PPR (8U)

void PgOut_Init(void);
void PgOut_Run(void);
void PgOut_UpdateState(void);
void PgOut_Reset(void);