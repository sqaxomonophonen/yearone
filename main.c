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

static float eccentric_anomaly_from_mean_anomaly(float M, float eccentricity, int iterations)
{
	float E = M;
	for (int i = 0; i < iterations; i++) {
		E = M + eccentricity * sinf(E);
	}
	return E;
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

static void kepler_calc_position(struct celestial_body* body, struct celestial_body* parent, float t, float* dx, float* dy)
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
	float t;
};


void world_init(struct world* world, struct celestial_body* sol)
{
	memset(world, 0, sizeof(*world));
	world->sol = sol;
}

struct observer {
	float height_km;
};

void observer_init(struct observer* observer)
{
	memset(observer, 0, sizeof(*observer));
	observer->height_km = 200000000;
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

	struct shader path_shader;
	GLuint path_a_position;
	GLuint path_a_uv;
	GLuint path_u_mu;
	GLuint path_u_color1;
	GLuint path_u_color2;

	struct shader sun_shader;
	GLuint sun_a_position;
	GLuint sun_u_offset;
	GLuint sun_u_scale;
	GLuint sun_u_mu;
	GLuint sun_u_color;

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

	SDL_GetWindowSize(render->window, &render->window_width, &render->window_height);
	glViewport(0, 0, render->window_width, render->window_height);

	{ /* path shader */
		#include "path.glsl.inc"
		shader_init(&render->path_shader, path_vert_src, path_frag_src);
		shader_use(&render->path_shader);
		render->path_a_position = glGetAttribLocation(render->path_shader.program, "a_position"); CHKGL;
		render->path_a_uv = glGetAttribLocation(render->path_shader.program, "a_uv"); CHKGL;
		render->path_u_mu = glGetUniformLocation(render->path_shader.program, "u_mu"); CHKGL;
		render->path_u_color1 = glGetUniformLocation(render->path_shader.program, "u_color1"); CHKGL;
		render->path_u_color2 = glGetUniformLocation(render->path_shader.program, "u_color2"); CHKGL;
	}

	{ /* sun shader */
		#include "sun.glsl.inc"
		shader_init(&render->sun_shader, sun_vert_src, sun_frag_src);
		shader_use(&render->sun_shader);
		render->sun_a_position = glGetAttribLocation(render->sun_shader.program, "a_position"); CHKGL;
		render->sun_u_offset = glGetUniformLocation(render->sun_shader.program, "u_offset"); CHKGL;
		render->sun_u_scale = glGetUniformLocation(render->sun_shader.program, "u_scale"); CHKGL;
		render->sun_u_mu = glGetUniformLocation(render->sun_shader.program, "u_mu"); CHKGL;
		render->sun_u_color = glGetUniformLocation(render->sun_shader.program, "u_color"); CHKGL;
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
}

void render_sun(struct render* render, struct celestial_body* sun, float x, float y)
{
	shader_use(&render->sun_shader);
	{
		float dx = x / render->window_width * 2;
		float dy = y / render->window_height * 2;
		glUniform2f(render->sun_u_offset, dx, dy); CHKGL;

		float radius = sun->mock_radius;
		float sx = radius / render->window_width * 2;
		float sy = radius / render->window_height * 2;
		glUniform2f(render->sun_u_scale, sx, sy); CHKGL;

		float mu = 4.0/(radius*6.0);
		glUniform1f(render->sun_u_mu, mu); CHKGL;

		glUniform3f(
			render->sun_u_color,
			sun->color[0],
			sun->color[1],
			sun->color[2]); CHKGL;
	}

	glEnableVertexAttribArray(render->sun_a_position); CHKGL;
	glBindBuffer(GL_ARRAY_BUFFER, render->quad_vertex_buffer); CHKGL;
	glVertexAttribPointer(render->sun_a_position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0); CHKGL;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->quad_index_buffer); CHKGL;
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, NULL); CHKGL;
	glDisableVertexAttribArray(render->sun_a_position); CHKGL;
}

void render_body(struct render* render, struct celestial_body* body, float x, float y)
{
	shader_use(&render->body_shader);
	{
		float dx = x / render->window_width * 2;
		float dy = y / render->window_height * 2;
		glUniform2f(render->body_u_offset, dx, dy); CHKGL;

		float radius = body->mock_radius;
		float sx = radius / render->window_width * 2;
		float sy = radius / render->window_height * 2;
		glUniform2f(render->body_u_scale, sx, sy); CHKGL;

		float mu = 4.0/(radius*6.0);
		glUniform1f(render->body_u_mu, mu); CHKGL;

		float lx = -x;
		float ly = -y;
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

void render_orbit(struct render* render, struct celestial_body* body, float scale, float cx, float cy)
{
	render_prim_reset(render);

	float width = 6;

	int N = 256;
	float a = body->semi_major_axis_km;
	float e = body->eccentricity;
	float b =  a * sqrtf(1 - e*e);
	for (int i = 0; i <= N; i++) {
		float E = (float)(i%N)/(float)N*TAU;
		float Ex = (float)i/(float)N*12.0f;
		float x,y,nx,ny;
		calc_ellipse_position(E, e, a, b, body->longitude_of_periapsis_rad, &x, &y, &nx, &ny);

		x = x * scale / render->window_width * 2;
		y = y * scale / render->window_height * 2;

		float dn = 1.0/sqrtf(nx*nx + ny*ny);
		nx = nx * dn / render->window_width * 2 * width;
		ny = ny * dn / render->window_height * 2 * width;

		float xs[] = {
			x - nx, y - ny, Ex, -1,
			x + nx, y + ny, Ex, 1
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
		float mux = 0.005f;
		float muy = 0.1f;
		glUniform2f(render->path_u_mu, mux, muy);

		float r = body->color[0];
		float g = body->color[1];
		float b = body->color[2];

		glUniform4f(render->path_u_color1, r, g, b, 0.5f);
		glUniform4f(render->path_u_color2, r, g, b, 0.2f);
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

void render_celestial_body(struct render* render, struct celestial_body* body, float scale, float t, float cx, float cy)
{
	ASSERT(body->n_satellites == 0 || body->satellites != NULL);
	for (int i = 0; i < body->n_satellites; i++) {
		struct celestial_body* child = &body->satellites[i];
		float dx,dy;
		kepler_calc_position(child, body, t, &dx, &dy);
		render_celestial_body(render, child, scale, t, cx + dx * scale, cy + dy * scale);
	}

	switch (body->renderer) {
		case CBR_SUN:
			render_sun(render, body, cx, cy);
			break;
		case CBR_BODY:
			render_orbit(render, body, scale, cx, cy);
			render_body(render, body, cx, cy);
			break;
	}
}

void render_world(struct render* render, struct world* world, struct observer* observer)
{
	float scale = (float)render->window_height / observer->height_km;
	render_celestial_body(render, world->sol, scale, world->t, 0, 0);
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
		0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL
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
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHKGL;

	struct render render;
	render_init(&render, window);

	struct world world;
	world_init(&world, sol);

	struct observer observer;
	observer_init(&observer);

	float lx = 1;
	float ly = 0;
	int exiting = 0;
	while (!exiting) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_QUIT:
					exiting = 1;
					break;
				case SDL_KEYDOWN:
					if (e.key.keysym.sym == SDLK_ESCAPE) exiting = 1;
					break;
				case SDL_MOUSEMOTION:
					lx = e.motion.x - 1920/2;
					ly = e.motion.y - 1080/2;
					break;
				case SDL_MOUSEWHEEL:
					observer.height_km *= powf(0.9, e.wheel.y);
					break;
			}
		}

		glClearColor(0,0,0,1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		world.t += 10000;

		render_world(&render, &world, &observer);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);

	return 0;
}
