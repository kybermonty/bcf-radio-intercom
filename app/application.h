#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef FIRMWARE
#define FIRMWARE "intercom"
#endif

#ifndef VERSION
#define VERSION "1.0"
#endif

#include <bcl.h>

typedef struct
{
    uint8_t channel;
    float value;
    bc_tick_t next_pub;

} event_param_t;

#endif
