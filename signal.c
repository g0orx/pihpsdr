/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <math.h>

#include "radio.h"
#include "signal.h"

#define PI 3.1415926535897932

double sineWave(double* buf, int samples, double sinphase, double freq) {
    double phaseStep = freq / (double)sample_rate * 2.0 * PI;
    double cosval = cos(sinphase);
    double sinval = sin(sinphase);
    double cosdelta = cos(phaseStep);
    double sindelta = sin(phaseStep);
    int i;

    for (i = 0; i < samples; i++) {
        double tmpval = cosval * cosdelta - sinval * sindelta;
        sinval = cosval * sindelta + sinval * cosdelta;
        cosval = tmpval;
        buf[i*2] = sinval;
        sinphase += phaseStep;
    }
    return sinphase;
}

double cosineWave(double* buf, int samples, double cosphase, double freq) {
    double phaseStep = freq / (double)sample_rate * 2.0 * PI;
    double cosval = cos(cosphase);
    double sinval = sin(cosphase);
    double cosdelta = cos(phaseStep);
    double sindelta = sin(phaseStep);
    int i;

    for (i = 0; i < samples; i++) {
        double tmpval = cosval * cosdelta - sinval * sindelta;
        sinval = cosval * sindelta + sinval * cosdelta;
        cosval = tmpval;
        buf[(i*2)+1] = cosval;
        cosphase += phaseStep;
    }
    return cosphase;
}
