#include "common.h"

#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include "Coord.h"
#include "Fixed.h"

Coord Coord::s_world(ONE);

/* work areas of libgte's polygon clipper */
EVECTOR evbfad[16];
EVECTOR* evmx[20];

/* `fov`: its raw value is the camera's field of view (an angle, 4096 a full turn) times 360 */
void SetProjection(Fixed fov)
{
    int angle = (fov / 360 / 2).Raw();
    int h = ((rcos(angle) * VWD0 / rsin(angle)) >> 1) * HWD0 * 3 / (VWD0 * 4);
    GsSetProjection(h);
    InitClip(evbfad, HWD0 + 8, VWD0 + 8, h, h / 2, 0xFFFF);
}

/* Clips the line from v0 to v1 to the view; false if nothing is left. */
int ClipLine(SVECTOR* v0, SVECTOR* v1)
{
    if (Clip3F(v0, v1, v1, evmx) < 2) {
        return 0;
    }
    *v0 = evmx[0]->v;
    *v1 = evmx[1]->v;
    return 1;
}
