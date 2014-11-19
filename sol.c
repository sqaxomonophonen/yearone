#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a.h"
#include "m.h"
#include "sol.h"

/*
https://en.wikipedia.org/wiki/Mean_longitude
http://nssdc.gsfc.nasa.gov/planetary/factsheet/
http://www.stargazing.net/kepler/kepler.html
*/

#define MAX_LEVELS (3)

#define MASS (1<<0)
#define RADIUS (1<<1)
#define SIDEREAL_ROTATION_PERIOD (1<<2)
#define SEMI_MAJOR_AXIS (1<<3)
#define ECCENTRICITY (1<<4)


enum {
	MODE_COUNT = 4242,
	MODE_MK = 6669
} mode;

int level;
int n_bodies;
int n_chars;

void* blob;
char* chars;
static struct celestial_body* bodies;
static struct celestial_body* cbody;
static struct celestial_body* cbody_stack[MAX_LEVELS];

static int* cflags;
static int cflags_stack[MAX_LEVELS];
static int expected_cflags_stack[MAX_LEVELS];

struct swoozle {
	int level;
	int n;
}* swoozle;

static void rgb(float r, float g, float b);
static void mock_radius(float r);

static void _begin(const char* name, int expected_cflags)
{
	ASSERT(mode == MODE_COUNT || mode == MODE_MK);

	if (mode == MODE_MK) {
		cbody = &bodies[n_bodies];
		if (level > 0) {
			struct celestial_body* parent = cbody_stack[level - 1];
			if (parent->satellites == NULL) {
				parent->satellites = cbody;
			}
			parent->n_satellites++;
		}
		cbody_stack[level] = cbody;
		cbody->name = chars + n_chars;
		strcpy(cbody->name, name);
		rgb(1,0,1);
		mock_radius(16);

		swoozle[n_bodies].level = level;
		swoozle[n_bodies].n = n_bodies;
	}

	n_bodies++;
	n_chars += strlen(name) + 1;

	cflags = &cflags_stack[level];
	*cflags = 0;
	expected_cflags_stack[level] = expected_cflags;

	ASSERT(level >= 0 && level < MAX_LEVELS);
	level++;
}

static void begin_sun(const char* name)
{
	_begin(name, MASS | RADIUS | SIDEREAL_ROTATION_PERIOD);
	if (cbody == NULL) return;
	cbody->renderer = CBR_SUN;
}

static void _begin_body(const char* name)
{
	_begin(name, MASS | RADIUS | SIDEREAL_ROTATION_PERIOD | SEMI_MAJOR_AXIS | ECCENTRICITY);
	if (cbody == NULL) return;
	cbody->renderer = CBR_BODY;
}

static void begin_planet(const char* name)
{
	_begin_body(name);
}

static void begin_moon(const char* name)
{
	_begin_body(name);
}

static void end()
{
	ASSERT(level > 0 && level <= MAX_LEVELS);
	level--;
	ASSERT(cflags_stack[level] == expected_cflags_stack[level]);
	cflags = &cflags_stack[level];
	cbody = cbody_stack[level];
}

static void mass_kg(float kg)
{
	*cflags |= MASS;
	if (cbody == NULL) return;
	cbody->mass_kg = kg;
}

static void radius_km(float km)
{
	*cflags |= RADIUS;
	if (cbody == NULL) return;
	cbody->radius_km = km;
}

static void sidereal_rotation_period_days(float d)
{
	*cflags |= SIDEREAL_ROTATION_PERIOD;
	if (cbody == NULL) return;
	cbody->sidereal_rotation_period_days = d;
}

static void synchronous_rotation()
{
	sidereal_rotation_period_days(0);
}

static void semi_major_axis_km(float km)
{
	*cflags |= SEMI_MAJOR_AXIS;
	if (cbody == NULL) return;
	cbody->semi_major_axis_km = km;
}

static void semi_major_axis_au(float au)
{
	semi_major_axis_km(au * AU_IN_KM);
}

static void eccentricity(float e)
{
	*cflags |= ECCENTRICITY;
	if (cbody == NULL) return;
	cbody->eccentricity = e;
}

static void longitude_of_periapsis_deg(float deg)
{
	if (cbody == NULL) return;
	cbody->longitude_of_periapsis_rad = DEG2RAD(deg);
}

static void mean_longitude_j2000_deg(float deg)
{
	if (cbody == NULL) return;
	cbody->mean_longitude_j2000_rad = DEG2RAD(deg);
}

static void rgb(float r, float g, float b)
{
	if (cbody == NULL) return;
	cbody->color[0] = r;
	cbody->color[1] = g;
	cbody->color[2] = b;
}

static void mock_radius(float r)
{
	if (cbody == NULL) return;
	cbody->mock_radius = r;
}

static void emit_bodies()
{
	begin_sun("sol");
		mass_kg(1.98855e30);
		radius_km(696342);
		sidereal_rotation_period_days(25.05);
		rgb(1,0,1);
		mock_radius(64);

		begin_planet("mercury");
			mass_kg(3.3022e23);
			radius_km(2439.7);
			semi_major_axis_au(0.387098);
			eccentricity(0.205630);
			sidereal_rotation_period_days(58.646);
			longitude_of_periapsis_deg(77.45645);
			mean_longitude_j2000_deg(252.25084);
			rgb(0.7, 0.3, 0.3);
			mock_radius(12);
		end();

		begin_planet("venus");
			mass_kg(4.8676e24);
			radius_km(6051.8);
			semi_major_axis_au(0.723327);
			eccentricity(0.0067);
			sidereal_rotation_period_days(-243.0185);
			longitude_of_periapsis_deg(131.53298);
			mean_longitude_j2000_deg(181.97973);
			rgb(0.9, 0.92, 0.9);
			mock_radius(16);
		end();

		begin_planet("earth");
			mass_kg(5.97219e24);
			radius_km(6378.1);
			semi_major_axis_au(1);
			eccentricity(0.01671123);
			sidereal_rotation_period_days(0.99726968);
			longitude_of_periapsis_deg(102.94719);
			mean_longitude_j2000_deg(100.46435);
			rgb(0.6, 0.8, 0.4);
			mock_radius(16);

			begin_moon("luna");
				mass_kg(7.3477e22);
				radius_km(1738.14);
				semi_major_axis_km(384399);
				eccentricity(0.0549);
				synchronous_rotation();
				rgb(0.4, 0.4, 0.4);
				mock_radius(10);
			end();
		end();

		begin_planet("mars");
			mass_kg(6.4185e23);
			radius_km(3396.2);
			semi_major_axis_au(1.523679);
			eccentricity(0.0934);
			sidereal_rotation_period_days(1.025957);
			longitude_of_periapsis_deg(336.04084);
			mean_longitude_j2000_deg(355.45332);
			rgb(0.9, 0.5, 0.0);
			mock_radius(14);

			begin_moon("phobos");
				mass_kg(1.0659e16);
				radius_km(11.2667);
				semi_major_axis_km(9376);
				eccentricity(0.0151);
				synchronous_rotation();
				rgb(0.4, 0.4, 0.4);
				mock_radius(7);
			end();

			begin_moon("deimos");
				mass_kg(1.4762e15);
				radius_km(6.2);
				semi_major_axis_km(23463.2);
				eccentricity(0.00033);
				synchronous_rotation();
				rgb(0.4, 0.4, 0.4);
				mock_radius(6);
			end();
		end();

		begin_planet("jupiter");
			mass_kg(1.8986e27);
			radius_km(71492.0);
			semi_major_axis_au(5.204267);
			eccentricity(0.048775);
			sidereal_rotation_period_days(0.41354167);
			longitude_of_periapsis_deg(14.75385);
			mean_longitude_j2000_deg(34.40438);
			rgb(1.0, 0.7, 0.7);
			mock_radius(24);
		end();

		begin_planet("saturn");
			mass_kg(5.6846e26);
			radius_km(60268.0);
			semi_major_axis_au(9.5820172);
			eccentricity(0.055723219);
			sidereal_rotation_period_days(0.4404167);
			longitude_of_periapsis_deg(92.43194);
			mean_longitude_j2000_deg(49.94432);
			rgb(1.0, 0.7, 0.3);
			mock_radius(22);
		end();

		begin_planet("uranus");
			mass_kg(8.6810e25);
			radius_km(25259.0);
			semi_major_axis_au(19.189253);
			eccentricity(0.047220087);
			sidereal_rotation_period_days(0.71833);
			longitude_of_periapsis_deg(170.96424);
			mean_longitude_j2000_deg(313.23218);
			rgb(0.7, 0.8, 0.9);
			mock_radius(18);
		end();

		begin_planet("neptune");
			mass_kg(1.0243e26);
			radius_km(24764.0);
			semi_major_axis_au(30.070900);
			eccentricity(0.00867797);
			sidereal_rotation_period_days(0.6713);
			longitude_of_periapsis_deg(131.72169);
			mean_longitude_j2000_deg(304.88003);
			rgb(0.4, 0.6, 1.0);
			mock_radius(17);
		end();

		begin_planet("pluto");
			mass_kg(1.305e22);
			radius_km(1184.0);
			semi_major_axis_au(39.264);
			eccentricity(0.24880766);
			sidereal_rotation_period_days(-6.387230);
			longitude_of_periapsis_deg(224.06676);
			mean_longitude_j2000_deg(238.92881);
			rgb(0.4, 0.4, 0.4);
			mock_radius(10);
		end();

	end();
}


//#define DUMP_BODIES

#ifdef DUMP_BODIES
static void _celestial_body_dump_rec(struct celestial_body* bs, int n, int level)
{
	if (bs == NULL) return;
	for (int i = 0; i < n; i++) {
		struct celestial_body* b = &bs[i];
		for (int l = 0; l < level; l++) printf("  ");
		printf("%s : %e kg (%d)\n", b->name, b->mass_kg, b->n_satellites);
		_celestial_body_dump_rec(b->satellites, b->n_satellites, level + 1);
	}
}

static void celestial_body_dump(struct celestial_body* b)
{
	_celestial_body_dump_rec(b, 1, 0);
}
#endif


static int swoozle_cmp(const void* va, const void* vb)
{
	const struct swoozle* a = va;
	const struct swoozle* b = vb;
	int d1 = a->level - b->level;
	if (d1 != 0) return d1;
	return a->n - b->n;
}

struct celestial_body* mksol()
{
	mode = MODE_COUNT;
	level = 0;
	n_bodies = 0;
	n_chars = 0;
	emit_bodies();

	AZ(level);
	AN(n_bodies);
	AN(n_chars);

	int save_n_bodies = n_bodies;
	int save_n_chars = n_chars;

	size_t bodies_sz = n_bodies * sizeof(struct celestial_body);
	size_t total_sz = bodies_sz + n_chars;
	blob = malloc(total_sz);
	AN(blob);
	memset(blob, 0, total_sz);
	bodies = blob;
	chars = (char*)blob + bodies_sz;

	swoozle = malloc(n_bodies * sizeof(struct swoozle));
	AN(swoozle);

	mode = MODE_MK;
	level = 0;
	n_bodies = 0;
	n_chars = 0;
	emit_bodies();

	AZ(level);
	ASSERT(n_bodies == save_n_bodies);
	ASSERT(n_chars == save_n_chars);

	// juggle stuff around so that bodies are in breadth-first order
	// (otherwise celestial_body.satellites cannot function as an array)
	struct celestial_body* bodies2 = malloc(bodies_sz);
	AN(bodies2);
	qsort(swoozle, n_bodies, sizeof(struct swoozle), swoozle_cmp);
	for (int i = 0; i < n_bodies; i++) {
		struct swoozle* si = &swoozle[i];
		memcpy(&bodies2[i], &bodies[si->n], sizeof(struct celestial_body));
		if (bodies2[i].satellites == NULL) continue;
		for (int j = 0; j < n_bodies; j++) {
			struct swoozle* sj = &swoozle[j];
			int oi = bodies2[i].satellites - bodies;
			if (oi == sj->n) {
				bodies2[i].satellites = &bodies[j];
				break;
			}
		}
	}
	memcpy(bodies, bodies2, bodies_sz);
	free(swoozle);
	free(bodies2);

	#ifdef DUMP_BODIES
	celestial_body_dump(bodies);
	AZ(1);
	#endif

	return bodies;
}

