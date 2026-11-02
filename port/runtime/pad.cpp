/*
 * libpad: the controllers' buffers, filled at every vertical blank, as the
 * BIOS does. The first port has a digital controller, run by the page
 * (keyboard or gamepad); the second has none.
 */
#include <string.h>

#include "psx.h"

static unsigned char* s_buffers[2];
static int s_started;

extern "C" {

void pad_vsync(void)
{
    if (!s_started) {
        return;
    }
    for (int port = 0; port < 2; port++) {
        unsigned char* b = s_buffers[port];
        if (b == NULL) {
            continue;
        }
        if (port == 0) {
            int held = js_pad(0);
            unsigned short buttons = ~held & 0xFFFF;
            b[0] = 0;    /* valid */
            b[1] = 0x41; /* a digital controller, two bytes of data */
            b[2] = buttons & 0xFF;
            b[3] = buttons >> 8;
            b[4] = b[5] = b[6] = b[7] = 0x80;
        } else {
            b[0] = 0xFF; /* nothing connected */
            b[1] = 0;
            b[2] = b[3] = 0xFF;
        }
    }
}

void PadInitDirect(unsigned char* buffer1, unsigned char* buffer2)
{
    s_buffers[0] = buffer1;
    s_buffers[1] = buffer2;
    if (buffer1) {
        memset(buffer1, 0xFF, 8);
    }
    if (buffer2) {
        memset(buffer2, 0xFF, 8);
    }
}

void PadStartCom(void)
{
    s_started = 1;
}

void PadStopCom(void)
{
    s_started = 0;
}

int PadGetState(int port)
{
    return port == 0 ? 6 /* PadStateStable */ : 0 /* PadStateDiscon */;
}

int PadSetActAlign(int port, unsigned char* data)
{
    return 1;
}

void PadSetAct(int port, unsigned char* data, int len) {}

} /* extern "C" */
