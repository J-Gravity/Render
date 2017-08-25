
#include "engine.h"
#include <stdio.h>
#include <SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>

static void				grav_exit()
{
	if (g_render->ren)
		SDL_DestroyRenderer(g_render->ren);
	if (g_render->win)
		SDL_DestroyWindow(g_render->win);
	delete	g_render;
	SDL_Quit();
	exit(0);
}

template <class ttype>
static inline void		check_error(ttype check, std::string message)
{
	if (check == (ttype)0)
	{
		std::cerr << message;
		grav_exit();
	}
}

Render::Render(int w, int h, std::string in, std::string out, int excl)
{
	this->width = w;
	this->height = h;
	this->in_path = in;
	this->out_path = out;
	this->scale = 2000000000000000.0;
	this->excl = excl;
	check_error(!SDL_Init(SDL_INIT_EVERYTHING), "SDL Init error\n");
	this->win = SDL_CreateWindow("J-Gravity", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
	check_error(this->win, "SDL Window creation error\n");
	this->ren = SDL_CreateRenderer(this->win, -1, SDL_RENDERER_ACCELERATED
		| SDL_RENDERER_PRESENTVSYNC);
	check_error(this->ren, "SDL Render create error\n");
	this->cam = new Camera(this->width, this->width,
		new Vector(0, 0, 0.0), new Matrix(ROTATION, 0.0, 0.0, 0.0),
		60, 1000.0, 500000.0);
	this->tick = 0;
	Color::set_range(-4.0, 4.0);
	Color::init_color_table(RAINBOW, 255);
	init_threading();
}

static inline long		h_read_long(FILE *f)
{
	long			out;

	fread((void*)(&out), sizeof(out), 1, f);
	return out;
}

static int				h_digits(int i)
{
	if (i < 10 && i >= 0)
		return 1;
	else if (i < 100 && i >= 0)
		return 2;
	else if (i < 1000 && i >= 0)
		return 3;
	else if (i < 10000 && i >= 0)
		return 4;
	else if (i < 100000 && i >= 0)
		return 5;
	else if (i < 1000000 && i >= 0)
		return 6;
	else if (i < 10000000 && i >= 0)
		return 7;
	else
	{
		std::cerr << "Ridiculous frame count achieved.\n";
		grav_exit();
		return (0);
	}
}

static inline unsigned char		h_avg(unsigned char a, unsigned char b)
{
	int					out;

	out = (a + b) / 2;
	return (unsigned char)out;
}

static inline double	h_read_double(FILE *f)
{
	float			out;
	size_t			r;

	r = fread((void*)(&out), sizeof(out), 1, f);
	check_error(r == 1, "Error reading from file");
	return (double)out;
}

static inline void		h_set_vars(FILE *f, Vector *vec, double &mass)
{
	char				*buf;
	size_t			r;

	buf = new char[16];
	r = fread((void*)buf, 1, 16, f);
	check_error(r > 0, "Error reading from file");
	vec->x = (double)(*(float*)(&buf[0]));
	vec->y = (double)(*(float*)(&buf[4]));
	vec->z = (double)(*(float*)(&buf[8]));
	mass = (double)(*(float*)(&buf[12]));
	delete buf;
}


float					*getBodies(int fd, size_t size, float *ret)
{
	if ((size * sizeof(float) * 4) != read(fd, ret, sizeof(float) * size * 4))
		dprintf(2, "Danger Will Robinson");
	return (ret);
}

//the real draw of this file. hon hon hon.
void					Render::draw(size_t size, float *bodies)
{
	//grab our bodies, the long at the beginning is already filtered out.
	size_t			i = 0;
	
	SDL_GL_SwapWindow(screen);
	//no idea what these do aside from setting which matrix I'm using but I know I'm supposed to use them
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//aspect ratio stuff
	gluPerspective(70, (double)640/480, 1, 1000);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS); //save the state of the OpenGL stuff
	glPushMatrix();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	gluLookAt(3, 4, 2, 0, 0, 0, 0, 0, 1); //3d math stuff
	glPointSize(3.0); //diameter of our points.
	glBegin(GL_POINTS); //treat all our vertexes as single points.
	//this works because 64 bits is really really big. Also like realistically speaking I'd be surprised if we broke 1 billion bodies, considering we are only rendering the centers of gravity of the tree's leaves.
	size = size * 4;
	while (i < size)
	{
		//I don't know how to scale colors.
		glColor3ub((int)(bodies[i + 3]) % 255, (int)(bodies[i + 3]) % 255, (int)(bodies[i + 3]) % 255);
		//and then put the bodies in as points
		glVertex3d(bodies[i + 0], bodies[i + 1], bodies[i + 2]);
		i += 4;
	}
	glEnd(); //this be straight magic
	glFlush();
	SDL_GL_SwapWindow(screen);
	
	glPopMatrix();
	glPopAttrib();
	//we don't need to free our bodies here because we are just going to overwrite it next time.
}


//Pablo doesn't bother commenting his code so I'm just going to overload draw and do something simpler while preserving his code in case we need something from it.
void					Render::draw(bool first)
{
	FILE				*fin;
	FILE				*fout;
	std::string			name;
	long				par;
	int					a, b, c, d;
	Color				*color;
	int					max;

	name = this->in_path + std::to_string(this->tick) + ".jgrav";
	fin = fopen(name.c_str(), "rb");
	SDL_SetRenderDrawColor(this->ren, 0, 0, 0, 0);
	SDL_RenderClear(this->ren);
	par = h_read_long(fin);
	// if (first)
	// 	this->scale = 1000.0 * (double)h_read_long(fin);
	organize_threads(par);
	fclose(fin);
	SDL_mutexP(this->mutex);
	SDL_CondBroadcast(this->start_cond);
	SDL_CondWait(this->done_cond, this->mutex);
	SDL_mutexV(this->mutex);
	a = 0;
	b = 0;
	c = 0;
	d = 0;
	for (int off = 0; off < this->pixels.size(); off += 4)
	{
		a = this->threads[0]->compix[off / 4];
		max = a;
		b = this->threads[1]->compix[off / 4];
		max = b > max ? b : max;
		c = this->threads[2]->compix[off / 4];
		max = c > max ? c : max;
		d = this->threads[3]->compix[off / 4];
		max = d > max ? d : max;
		if (max > 0)
		{
			color = Color::table[max - 1];
			this->pixels[off + 0] = color->b;
			this->pixels[off + 1] = color->g;
			this->pixels[off + 2] = color->r;
			this->pixels[off + 3] = SDL_ALPHA_OPAQUE;
			this->compix[off / 4] = max;
		}
	}
	SDL_UpdateTexture(this->tex, NULL, &this->pixels[0], this->width * 4);
	SDL_RenderCopy(this->ren, this->tex, NULL, NULL);
	if (this->to_gif)
	{
		name = this->out_path + std::to_string(this->tick) + ".jgpix";
		fout = fopen(name.c_str(), "w");
		fwrite((const char*)&this->compix[0], this->compix.size(),
			sizeof(char), fout);
		fclose(fout);
	}
	SDL_RenderPresent(this->ren);
}

/*static void				h_update_from_input(Render *r, bool *u)
{
	bool				update;

	update = false;
	if (r->keystate[SDL_SCANCODE_W] && (update = true) == true)
		r->cam->mod_angles(M_PI / 32.0, 0.0, 0.0);
	if (r->keystate[SDL_SCANCODE_S] && (update = true) == true)
		r->cam->mod_angles(-M_PI / 32.0, 0.0, 0.0);
	if (r->keystate[SDL_SCANCODE_A] && (update = true) == true)
		r->cam->mod_angles(0.0, -M_PI / 32.0, 0.0);
	if (r->keystate[SDL_SCANCODE_D] && (update = true) == true)
		r->cam->mod_angles(0.0, M_PI / 32.0, 0.0);
	if (r->keystate[SDL_SCANCODE_Q] && (update = true) == true)
		r->cam->mod_angles(0.0, 0.0, -M_PI / 32.0);
	if (r->keystate[SDL_SCANCODE_E] && (update = true) == true)
		r->cam->mod_angles(0.0, 0.0, M_PI / 32.0);
	if (r->keystate[SDL_SCANCODE_UP] && (update = true) == true)
		r->cam->mod_location(0.0, -r->scale / 200.0, 0.0);
	if (r->keystate[SDL_SCANCODE_DOWN] && (update = true) == true)
		r->cam->mod_location(0.0, r->scale / 200.0, 0.0);
	if (r->keystate[SDL_SCANCODE_LEFT] && (update = true) == true)
		r->cam->mod_location(-r->scale / 200.0, 0.0, 0.0);
	if (r->keystate[SDL_SCANCODE_RIGHT] && (update = true) == true)
		r->cam->mod_location(r->scale / 200.0, 0.0, 0.0);
	if (update)
		*u = 0;
}*/

void					Render::loop(long start, long end)
{
	bool				running;
	bool				paused;
	bool				updated;
	bool				first;
	int					dir;
	SDL_Event			event;
	size_t				size = 0;
	int					fd;
	std::string			name;
	float				**bodies;

	this->tick = start;
	this->start_tick = start;
	running = 1;
	paused = 1;
	updated = 0;
	dir = 1;
	first = true;
	while (running)
	{
		if (!paused)
		{
			this->tick += dir;
			updated = 0;
		}
		if (this->tick == end || this->tick == 0)
			paused = 1;
		if (this->tick < start)
		{
			updated = 0;
			this->tick = start;
		}
		else if (this->tick > end)
		{
			updated = 0;
			this->tick = end;
		}
		if (!updated)
		{
			name = this->in_path + std::to_string(this->tick) + ".jgrav";
			fd = open(name.c_str(), O_RDONLY);
			read(fd, &size, sizeof(long));
			if (first)
			{
				//initialize our frames and populate them. Improve this by doing the populating and the rendering in different threads.
				//Ideally we would populate up to x frames and then start rendering, with the rendering pausing if we outpace the population. We should render multiple frames of the simulation intot the future. There should be a way to set a framerate as well.
				//doing it this way makes a lot of sense if you actually think about what malloc does
				bodies = (float**)malloc(sizeof(float*) * FRAME_COUNT);
				for (size_t j = 0; j < FRAME_COUNT; j++)
				{
					bodies[j] = (float*)malloc(sizeof(float) * size * 4);
					bodies[j] = getBodies(fd, size, bodies[j]);
				}
				for (size_t j = 0; j < FRAME_COUNT; j++)
				{
					this->draw(size, bodies[j]);
					first = false;
				}
			}
			else
			{
				for (size_t j = 0; j < FRAME_COUNT; j++)
				{
					bodies[j] = getBodies(fd, size, bodies[j]);
				}
				for (size_t j = 0; j < FRAME_COUNT; j++)
				{
					this->draw(size, bodies[j]);
				}
			}
			close(fd);
			updated = 1;
		}
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					running = 0;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)
					{
						case SDLK_ESCAPE:
							running = 0;
							break;
						case SDLK_SPACE:
							paused = !paused;
							break;
						case SDLK_b:
							if (this->tick > start)
								this->tick--;
							updated = 0;
							break;
						case SDLK_n:
							if (this->tick < end)
								this->tick++;
							updated = 0;
							break;
						case SDLK_m:
							this->tick = start;
							updated = 0;
							break;
						case SDLK_r:
							dir *= -1;
							break;
						default:
							break;
					}
					break;
				case SDL_MOUSEWHEEL:
					if (event.wheel.y < 0)
						this->scale *= 1.1;
					else if (event.wheel.y > 0)
						this->scale /= 1.1;
					updated = 0;
					break;
			}
		}
		this->keystate = (Uint8*)SDL_GetKeyboardState(NULL);
		//h_update_from_input(this, &updated);
	}
	this->end_tick = this->tick;
}

void					Render::wait_for_death()
{
	SDL_Event			e;

	while (1)
		while (SDL_PollEvent(&e))
			switch (e.type)
			{
				case SDL_KEYDOWN:
				case SDL_QUIT:
					grav_exit();
					break;
			}
}

Render::~Render()
{
}
