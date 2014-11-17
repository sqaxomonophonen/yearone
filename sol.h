#ifndef SOL_H
#define SOL_H

#include "m.h"

#define AU_IN_KM (149597870.7)

/*
#define AU2KM(au) (au * AU_IN_KM)
*/

struct celestial_body {
	char* name;
	struct celestial_body* satellites;
	int n_satellites;
	float mass_kg;
	float radius_km;
	float semi_major_axis_km;
	float eccentricity;
	float sidereal_rotation_period_days; // 0==synchronous
	float longitude_of_periapsis_rad;
	float mean_longitude_j2000_rad;
	float color[3];
	enum {
		CBR_SUN,
		CBR_BODY
	} renderer;
};

struct celestial_body* mksol();

#endif/*SOL_H*/
