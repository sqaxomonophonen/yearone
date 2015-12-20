#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <SDL.h>
#include <GL/glew.h>

#include "a.h"
#include "m.h"
#include "shader.h"
#include "mud.h"
#include "sol.h"
#include "text.h"

static inline float lerpf(float t, float x0, float x1)
{
	return x0 + (x1-x0)*t;
}

static inline float clampf(float v, float min, float max)
{
	return v < min ? min : v > max ? max : v;
}

static float eccentric_anomaly_from_mean_anomaly(float M, float eccentricity, int iterations)
{
	float E = M;
	for (int i = 0; i < iterations; i++) {
		E = M + eccentricity * sinf(E);
	}
	return E;
}

static float mean_anomaly_from_eccentric_anomaly(float E, float eccentricity)
{
	return E - eccentricity * sinf(E);
}

static void calc_ellipse_position(
	float eccentric_anomaly,
	float eccentricity,
	float semi_major_axis,
	float semi_minor_axis,
	float longitude_of_periapsis,
	float* x,
	float* y,
	float* nx,
	float* ny)
{
	float Bx = cosf(longitude_of_periapsis);
	float By = sinf(longitude_of_periapsis);

	float Ex = (cosf(eccentric_anomaly) - eccentricity) * semi_major_axis;
	float Ey = sinf(eccentric_anomaly) * semi_minor_axis;
	if (x != NULL) *x = Bx * Ex - By * Ey;
	if (y != NULL) *y = By * Ex + Bx * Ey;

	float Nx = cosf(eccentric_anomaly) * semi_minor_axis;
	float Ny = sinf(eccentric_anomaly) * semi_major_axis;
	if (nx != NULL) *nx = Bx * Nx - By * Ny;
	if (ny != NULL) *ny = By * Nx + Bx * Ny;
}

static void kepler_calc_relative_position(struct celestial_body* body, struct celestial_body* parent, float t, float* dx, float* dy)
{
	float mu = G * parent->mass_kg;
	float a = body->semi_major_axis_km;
	float e = body->eccentricity;
	float b =  a * sqrtf(1 - e*e);
	float orbital_period = TAU * sqrtf(a*a*a / mu);
	float M0 = body->mean_longitude_j2000_rad - body->longitude_of_periapsis_rad;
	float M = M0 + (t / orbital_period) * TAU;
	float E = eccentric_anomaly_from_mean_anomaly(M, e, 10);
	calc_ellipse_position(E, e, a, b, body->longitude_of_periapsis_rad, dx, dy, NULL, NULL);
}


struct world {
	struct celestial_body* sol;
	int64_t t60;
};


static double world_t1(struct world* world)
{
	return (double)world->t60 / 60.0;
}

void world_init(struct world* world, struct celestial_body* sol)
{
	memset(world, 0, sizeof(*world));
	world->sol = sol;
}

struct observer {
	float height_km_target;
	float height_km;
	struct celestial_body* cbody;
	float cx, cy;
};

void observer_init(struct observer* observer)
{
	memset(observer, 0, sizeof(*observer));
}


#if 0
struct entity {
	int id;
	int parent_id;
	enum {
		ENT_BODY
	} type;

	union {
	};
};
#endif

struct render {
	SDL_Window* window;
	int window_width;
	int window_height;

	float scale;

	struct shader path_shader;
	GLuint path_a_position;
	GLuint path_a_uv;
	GLuint path_u_mu;
	GLuint path_u_color1;

	struct shader sun_shader;
	GLuint sun_a_position;
	GLuint sun_u_offset;
	GLuint sun_u_scale;
	GLuint sun_u_mu;

	struct shader body_shader;
	GLuint body_a_position;
	GLuint body_u_offset;
	GLuint body_u_scale;
	GLuint body_u_mu;
	GLuint body_u_light;
	GLuint body_u_color;

	GLuint quad_vertex_buffer;
	GLuint quad_index_buffer;

	GLuint prim_vertex_buffer;
	float* prim_vertex_data;
	int prim_vertex_n;
	int prim_vertex_max;
	GLuint prim_index_buffer;
	uint32_t* prim_index_data;
	int prim_index_n;
	int prim_index_max;

	struct text text;
};


void render_prim_reset(struct render* render)
{
	render->prim_vertex_n = 0;
	render->prim_index_n = 0;
}

void render_prim_vertex_data(struct render* render, float* xs, int n)
{
	ASSERT((render->prim_vertex_n + n) <= render->prim_vertex_max);
	memcpy(render->prim_vertex_data + render->prim_vertex_n, xs, sizeof(float) * n);
	render->prim_vertex_n += n;
}

void render_prim_index_data(struct render* render, uint32_t* xs, int n)
{
	ASSERT((render->prim_index_n + n) <= render->prim_index_max);
	memcpy(render->prim_index_data + render->prim_index_n, xs, sizeof(uint32_t) * n);
	render->prim_index_n += n;
}

void render_init(struct render* render, SDL_Window* window)
{
	memset(render, 0, sizeof(*render));
	render->window = window;

	{ /* path shader */
		#include "path.glsl.inc"
		shader_init(&render->path_shader, path_vert_src, path_frag_src);
		shader_use(&render->path_shader);
		render->path_a_position = glGetAttribLocation(render->path_shader.program, "a_position"); CHKGL;
		render->path_a_uv = glGetAttribLocation(render->path_shader.program, "a_uv"); CHKGL;
		render->path_u_mu = glGetUniformLocation(render->path_shader.program, "u_mu"); CHKGL;
		render->path_u_color1 = glGetUniformLocation(render->path_shader.program, "u_color1"); CHKGL;
	}

	{ /* sun shader */
		#include "sun.glsl.inc"
		shader_init(&render->sun_shader, sun_vert_src, sun_frag_src);
		shader_use(&render->sun_shader);
		render->sun_a_position = glGetAttribLocation(render->sun_shader.program, "a_position"); CHKGL;
		render->sun_u_offset = glGetUniformLocation(render->sun_shader.program, "u_offset"); CHKGL;
		render->sun_u_scale = glGetUniformLocation(render->sun_shader.program, "u_scale"); CHKGL;
		render->sun_u_mu = glGetUniformLocation(render->sun_shader.program, "u_mu"); CHKGL;
	}

	{ /* body shader */
		#include "body.glsl.inc"
		shader_init(&render->body_shader, body_vert_src, body_frag_src);
		shader_use(&render->body_shader);
		render->body_a_position = glGetAttribLocation(render->body_shader.program, "a_position"); CHKGL;
		render->body_u_offset = glGetUniformLocation(render->body_shader.program, "u_offset"); CHKGL;
		render->body_u_scale = glGetUniformLocation(render->body_shader.program, "u_scale"); CHKGL;
		render->body_u_mu = glGetUniformLocation(render->body_shader.program, "u_mu"); CHKGL;
		render->body_u_light = glGetUniformLocation(render->body_shader.program, "u_light"); CHKGL;
		render->body_u_color = glGetUniformLocation(render->body_shader.program, "u_color"); CHKGL;
	}

	{ /* quad vertex buffer */
		glGenBuffers(1, &render->quad_vertex_buffer); CHKGL;
		glBindBuffer(GL_ARRAY_BUFFER, render->quad_vertex_buffer); CHKGL;
		float data[] = {-1,-1, 1,-1, 1,1, -1,1};
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW); CHKGL;
	}

	{ /* quad index buffer */
		glGenBuffers(1, &render->quad_index_buffer); CHKGL;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->quad_index_buffer); CHKGL;
		uint8_t data[] = {0,1,2,3};
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW); CHKGL;
	}

	{ /* primitive vertex buffer */
		glGenBuffers(1, &render->prim_vertex_buffer); CHKGL;
		glBindBuffer(GL_ARRAY_BUFFER, render->prim_vertex_buffer); CHKGL;
		render->prim_vertex_max = 65536;
		size_t sz = sizeof(float) * render->prim_vertex_max;
		AN(render->prim_vertex_data = malloc(sz));
		glBufferData(GL_ARRAY_BUFFER, sz, render->prim_vertex_data, GL_STREAM_DRAW); CHKGL;
	}

	{ /* primitive index buffer */
		glGenBuffers(1, &render->prim_index_buffer); CHKGL;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->prim_index_buffer); CHKGL;
		render->prim_index_max = 65536;
		size_t sz = sizeof(int32_t) * render->prim_index_max;
		AN(render->prim_index_data = malloc(sz));
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, render->prim_index_data, GL_STREAM_DRAW); CHKGL;
	}

	text_init(&render->text);
}

void render_sun(struct render* render, struct celestial_body* sun)
{
	shader_use(&render->sun_shader);
	{
		float x = sun->render_x;
		float y = sun->render_y;

		float dx = x / render->window_width * 2;
		float dy = y / render->window_height * 2;
		glUniform2f(render->sun_u_offset, dx, dy); CHKGL;

		float radius = sun->render_radius;
		float sx = radius / render->window_width * 2;
		float sy = radius / render->window_height * 2;
		glUniform2f(render->sun_u_scale, sx, sy); CHKGL;

		float mu = 4.0/(radius*6.0);
		glUniform1f(render->sun_u_mu, mu); CHKGL;
	}

	glEnableVertexAttribArray(render->sun_a_position); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->quad_vertex_buffer); CHKGL;
	glVertexAttribPointer(render->sun_a_position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->quad_index_buffer); CHKGL;
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, NULL); CHKGL;
	glDisableVertexAttribArray(render->sun_a_position); CHKGL;
}

void render_body(struct render* render, struct celestial_body* body)
{
	shader_use(&render->body_shader);
	{
		float x = body->render_x;
		float y = body->render_y;

		float dx = x / render->window_width * 2;
		float dy = y / render->window_height * 2;
		glUniform2f(render->body_u_offset, dx, dy); CHKGL;

		float radius = body->render_radius;
		float sx = radius / render->window_width * 2;
		float sy = radius / render->window_height * 2;
		glUniform2f(render->body_u_scale, sx, sy); CHKGL;

		float mu = 4.0/(radius*6.0);
		glUniform1f(render->body_u_mu, mu); CHKGL;

		float lx = -body->kepler_x;
		float ly = -body->kepler_y;
		float d = 1/sqrtf(lx*lx + ly*ly);
		glUniform2f(render->body_u_light, lx*d, ly*d); CHKGL;

		glUniform3f(
			render->body_u_color,
			body->color[0],
			body->color[1],
			body->color[2]); CHKGL;
	}

	glEnableVertexAttribArray(render->body_a_position); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->quad_vertex_buffer); CHKGL;
	glVertexAttribPointer(render->body_a_position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->quad_index_buffer); CHKGL;
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, NULL); CHKGL;
	glDisableVertexAttribArray(render->body_a_position); CHKGL;
}

void render_orbit(struct render* render, struct celestial_body* body)
{
	render_prim_reset(render);

	float width = 6;

	int N = 256;
	float a = body->semi_major_axis_km;
	float e = body->eccentricity;
	float b =  a * sqrtf(1 - e*e);
	for (int i = 0; i <= N; i++) {
		float E = (float)(i%N)/(float)N*TAU;
		float x,y,nx,ny;
		calc_ellipse_position(E, e, a, b, body->longitude_of_periapsis_rad, &x, &y, &nx, &ny);

		x = (body->parent->render_x + x * render->scale) / render->window_width * 2;
		y = (body->parent->render_y + y * render->scale) / render->window_height * 2;

		float dn = 1.0/sqrtf(nx*nx + ny*ny);
		nx = nx * dn / render->window_width * 2 * width;
		ny = ny * dn / render->window_height * 2 * width;

		float Mx = (mean_anomaly_from_eccentric_anomaly((float)i/(float)N*TAU, e) / TAU) * 12.0;
		float xs[] = {
			x - nx, y - ny, Mx, -1,
			x + nx, y + ny, Mx, 1
		};
		render_prim_vertex_data(render, xs, 8);

		if (i<N) {
			int i2 = i+1;
			uint32_t idxs[] = {(i<<1), (i<<1)+1, (i2<<1)+1, (i2<<1)};
			render_prim_index_data(render, idxs, 4);
		}
	}

	shader_use(&render->path_shader);

	{
		float mux = 0.003f;
		float muy = 0.1f;
		glUniform2f(render->path_u_mu, mux, muy);

		float grayd = 0.6;
		float r = lerpf(grayd, body->color[0], 0.4);
		float g = lerpf(grayd, body->color[1], 0.4);
		float b = lerpf(grayd, body->color[2], 0.4);

		glUniform4f(render->path_u_color1, r, g, b, 0.5f);
	}

	glEnableVertexAttribArray(render->path_a_position); CHKGL;
	glEnableVertexAttribArray(render->path_a_uv); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->prim_vertex_buffer); CHKGL;
	glBufferSubData(GL_ARRAY_BUFFER, 0, render->prim_vertex_n * sizeof(float), render->prim_vertex_data); CHKGL;
	glVertexAttribPointer(render->path_a_position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0); CHKGL;
	glVertexAttribPointer(render->path_a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (char*)(sizeof(float)*2)); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->prim_index_buffer); CHKGL;
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, render->prim_index_n * sizeof(uint32_t), render->prim_index_data); CHKGL;
	glDrawElements(GL_QUADS, render->prim_index_n, GL_UNSIGNED_INT, NULL); CHKGL;
	glDisableVertexAttribArray(render->path_a_uv); CHKGL;
	glDisableVertexAttribArray(render->path_a_position); CHKGL;
}

void render_celestial_body(struct render* render, struct celestial_body* body)
{
	ASSERT(body->n_satellites == 0 || body->satellites != NULL);
	for (int i = 0; i < body->n_satellites; i++) {
		struct celestial_body* child = &body->satellites[i];
		render_celestial_body(render, child);
	}

	switch (body->renderer) {
		case CBR_SUN:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			render_sun(render, body);
			break;
		case CBR_BODY:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;
			render_orbit(render, body);
			render_body(render, body);
			break;
	}
}

void render_world(struct render* render, struct world* world)
{
	SDL_GetWindowSize(render->window, &render->window_width, &render->window_height);
	glViewport(0, 0, render->window_width, render->window_height);

	render_celestial_body(render, world->sol);
}


void _update_body_kepler_position_rec(
	struct celestial_body* body,
	struct celestial_body* parent,
	float t,
	float x, float y)
{
	if (parent != NULL) {
		float dx, dy;
		kepler_calc_relative_position(body, parent, t, &dx, &dy);
		x += dx;
		y += dy;
		body->kepler_x = x;
		body->kepler_y = y;
	} else {
		body->kepler_x = 0;
		body->kepler_y = 0;
	}

	for (int i = 0; i < body->n_satellites; i++) {
		struct celestial_body* child = &body->satellites[i];
		_update_body_kepler_position_rec(child, body, t, x, y);
	}
}

void update_bodies_kepler_position(struct world* world)
{
	_update_body_kepler_position_rec(world->sol, NULL, world_t1(world), 0, 0);
}



void _update_body_screen_position_rec(
	struct celestial_body* body,
	float scale,
	float cx, float cy)
{
	body->render_x = (body->kepler_x - cx) * scale;
	body->render_y = (body->kepler_y - cy) * scale;

	float actual_radius = body->radius_km * scale;
	body->render_radius = actual_radius > body->mock_radius ? actual_radius : body->mock_radius;

	for (int i = 0; i < body->n_satellites; i++) {
		struct celestial_body* child = &body->satellites[i];
		_update_body_screen_position_rec(child, scale, cx, cy);
	}
}

void update_bodies_screen_position(struct render* render, struct world* world, struct observer* observer)
{
	_update_body_screen_position_rec(
		world->sol,
		render->scale,
		observer->cx, observer->cy
	);
}


void _render_time(struct render* render, struct world* world, int64_t t)
{
	struct text* tx = &render->text;
	int second = t%60;
	int minute = (t/60)%60;
	int hour = (t/(60*60))%24;
	int day = ((t/(60*60*24))%28)+1;
	int month = ((t/(60*60*24*28))%12)+1;
	int year = t/(60*60*24*28*12)+1;

	text_set_cursor(tx, 16, render->window_height - 16 - 24);
	text_printf(tx, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}

void render_time(struct render* render, struct world* world, int64_t dt60)
{
	int N = 10;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	struct text* tx = &render->text;
	text_set_window_dimensions(tx, render->window_width, render->window_height);
	text_set_font(tx, font_ter24);
	text_set_variant(tx, 1);
	float cs = 1.0 / (float)N;
	text_set_color3f(tx, 0.8*cs, 1*cs, 1*cs);

	for (int i = -N; i <= 0; i++) {
		int64_t t = (world->t60 + i*dt60/(N/3)) / 60;
		if (t>=0) _render_time(render, world, t);
	}
}

struct celestial_body* _find_body_at_screen_position_rec(struct render* render, struct celestial_body* body, int x, int y)
{
	float dx = (float)x - body->render_x - render->window_width/2;
	float dy = (float)y + body->render_y - render->window_height/2;
	float d = sqrtf(dx*dx + dy*dy);

	if (d < body->render_radius) {
		return body;
	}

	for (int i = 0; i < body->n_satellites; i++) {
		struct celestial_body* found = _find_body_at_screen_position_rec(render, &body->satellites[i], x, y);
		if (found != NULL) return found;
	}

	return NULL;
}

struct celestial_body* find_body_at_screen_position(struct render* render, struct world* world, int x, int y)
{
	return _find_body_at_screen_position_rec(render, world->sol, x, y);
}

int main(int argc, char** argv)
{
	struct celestial_body* sol = mksol();

	SAZ(SDL_Init(SDL_INIT_VIDEO));
	atexit(SDL_Quit);

	SDL_Window* window = SDL_CreateWindow(
		"year one",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		//1920,1080,SDL_WINDOW_OPENGL
		0, 0,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
	);
	SAN(window);

	SDL_GLContext glctx = SDL_GL_CreateContext(window);
	SAN(glctx);

	SAZ(SDL_GL_SetSwapInterval(1)); // or -1, "late swap tearing"?

	{
		GLenum err = glewInit();
		if (err != GLEW_OK) {
			arghf("glewInit() failed: %s", glewGetErrorString(err));
		}

		#define CHECK_GL_EXT(x) { if(!GLEW_ ## x) arghf("OpenGL extension not found: " #x); }
		CHECK_GL_EXT(ARB_shader_objects);
		CHECK_GL_EXT(ARB_vertex_shader);
		CHECK_GL_EXT(ARB_fragment_shader);
		CHECK_GL_EXT(ARB_framebuffer_object);
		CHECK_GL_EXT(ARB_vertex_buffer_object);
		#undef CHECK_GL_EXT

		/* to figure out what extension something belongs to, see:
		 * http://www.opengl.org/registry/#specfiles */

		// XXX check that version is at least 1.30?
		// printf("GLSL version %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	}

	glDisable(GL_DEPTH_TEST); CHKGL;
	glDisable(GL_CULL_FACE); CHKGL;

	glEnable(GL_BLEND); CHKGL;
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;

	fonts_init();

	struct render render;
	render_init(&render, window);

	struct world world;
	world_init(&world, sol);

	struct observer observer;
	observer_init(&observer);

	observer.cbody = sol;
	observer.height_km = observer.height_km_target = 3e8;

	SDL_Cursor* arrow_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	SDL_Cursor* click_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	SDL_SetCursor(arrow_cursor);

	int exiting = 0;
	while (!exiting) {
		int clicked = 0;

		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_QUIT:
					exiting = 1;
					break;
				case SDL_KEYDOWN:
					if (e.key.keysym.sym == SDLK_ESCAPE) exiting = 1;
					break;
				case SDL_MOUSEWHEEL:
					observer.height_km_target *= powf(0.95, e.wheel.y);
					break;
				case SDL_MOUSEBUTTONDOWN:
					clicked++;
					break;
			}
		}

		int mx = 0;
		int my = 0;
		SDL_GetMouseState(&mx, &my);

		observer.height_km += (observer.height_km_target - observer.height_km) * 0.4f;

		glClearColor(0,0,0,1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int64_t dt60 = 100000;
		world.t60 += dt60;
		render_time(&render, &world, dt60);

		text_flush(&render.text);

		update_bodies_kepler_position(&world);
		observer.cx = observer.cbody->kepler_x;
		observer.cy = observer.cbody->kepler_y;
		render.scale = (float)render.window_height / observer.height_km;
		update_bodies_screen_position(&render, &world, &observer);

		struct celestial_body* hover = find_body_at_screen_position(&render, &world, mx, my);
		if (hover != NULL) {
			SDL_SetCursor(click_cursor);
			if (clicked) observer.cbody = hover;
		} else {
			SDL_SetCursor(arrow_cursor);
		}

		render_world(&render, &world);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);

	return 0;
}
