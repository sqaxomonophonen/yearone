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
	struct celestial_body* parent;
	int n_satellites;
	float mass_kg;
	float mock_radius;
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

	float render_x;
	float render_y;
	float render_radius;
	float kepler_x;
	float kepler_y;
};

struct celestial_body* mksol();

#endif/*SOL_H*/
