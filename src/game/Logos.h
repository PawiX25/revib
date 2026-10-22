#ifndef LOGOS_H
#define LOGOS_H

#include "common.h"

#include "Fixed.h"

/* The Sony Computer Entertainment and NanaOn-Sha screens shown at start-up. */
void LoadLogos();
void FreeLogos();
/* times in seconds */
void ShowSonyLogos(Fixed fadeIn, Fixed hold, Fixed fadeOut);
void ShowNanaOnShaLogos(Fixed fadeIn, Fixed hold, Fixed fadeOut);

#endif
