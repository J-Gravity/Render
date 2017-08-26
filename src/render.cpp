
#include "engine.h"
#include <stdio.h>
#include <render.h>
#include <pthread.h>
#include <queue>

int		frames_buffered = 0;

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


float					*Render::getBodies(int fd, size_t size, float *ret)
{
	if ((size * sizeof(float) * 4) != read(fd, ret, sizeof(float) * size * 4))
		dprintf(2, "Danger Will Robinson");
	return (ret);
}

//the real draw of this file. hon hon hon.
void					Render::draw(size_t size, float *bodies, SDL_Window *screen)
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

void					Render::draw(size_t size, std::queue<float*> *bodies, SDL_Window *screen, size_t *i)
{
	float			*buf;
	if(*i < frames_buffered)
	{
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
		glBegin(GL_POINTS); //treat all our vertexes as single points

		buf = bodies->front();
		for (int j = 0; j < size * 4; j += 4)
		{
			glColor3ub((int)buf[j + 3] % 255, 0, 255);
			glVertex3d(buf[j + 0], buf[j + 1], buf[j + 2]);
		}
		free(buf);
		bodies->pop();
		*i += 1;
		glEnd(); //this be straight magic
		glFlush();
		SDL_GL_SwapWindow(screen);
	
		glPopMatrix();
		glPopAttrib();
	}
}


//Pablo doesn't bother commenting his code so I'm just going to overload draw and do something simpler while preserving his code in case we need something from it.


typedef struct s_frame_params
{
	size_t				*size;
	std::queue<float*>	*bodies;
	long				end;
}				t_frame_params;

void					Render::buffer_frames(void *p)
{
	int					first = 1;
	int					sub_frames = 0;
	int					fd;
	std::string			name;
	float				*frame;
	float				*buf;
	float				*prev;
	size_t				size;
	t_frame_params		*params = (t_frame_params*)p;
	
	name = this->in_path + std::to_string(this->tick) + ".jgrav";
	fd = open(name.c_str(), O_RDONLY);
	read(fd, params->size, sizeof(long));
	size = *(params->size);
	buf = (float*)malloc(sizeof(float) * size * 4);
	buf = getBodies(fd, size, buf);
	close(fd);
	prev = (float*)calloc(size, sizeof(float));
	//while we have not reached the end tick
	while (this->tick < params->end)
	{
		//buffer sixty frames in advance
		while (params->bodies->size() < 60)
		{
		if (!first)
		{
			//save our old buf in prev
			prev = (float*)memcpy(prev, buf, size * 4);
			name = this->in_path + std::to_string(this->tick) + ".jgrav";
			fd = open(name.c_str(), O_RDONLY);
			read(fd, &size, sizeof(long));
			//get our new buf
			buf = getBodies(fd, size, buf);
			close(fd);
		}
		first = 0;
		sub_frames = 1;
		while (sub_frames < FRAME_COUNT + 1)
		{
			frame = (float*)malloc(sizeof(float) * size * 4);
			for (int j = 0; j < size * 4; j += 4)
			{
				//create new frames that transition between the previous tick and this tick in 36 steps.
				if (buf[j + 0] < prev[j + 0])
					frame[j + 0] = prev[j + 0] - (((prev[j + 0] - buf[j + 0])/FRAME_COUNT) * sub_frames);
				else
					frame[j + 0] = prev[j + 0] + (((buf[j + 0] - prev[j + 0])/FRAME_COUNT) * sub_frames);
				if (buf[j + 1] < prev[j + 1])
					frame[j + 1] = prev[j + 1] - (((prev[j + 1] - buf[j + 1])/FRAME_COUNT) * sub_frames);
				else
					frame[j + 1] = prev[j + 1] + (((buf[j + 1] - prev[j + 1])/FRAME_COUNT) * sub_frames);
				if (buf[j + 2] < prev[j + 2])
					frame[j + 2] = prev[j + 2] - (((prev[j + 2] - buf[j + 2])/FRAME_COUNT) * sub_frames);
				else
					frame[j + 2] = prev[j + 2] + (((buf[j + 2] - prev[j + 2])/FRAME_COUNT) * sub_frames);
				frame[j + 3] = buf[j + 3];
			}
			//put the new frame at the end of our vector and increment the buffered frames count
			params->bodies->push(frame);
			sub_frames++;
			frames_buffered++;
		}
		//go to the next file
		this->tick++;
		}
	}
}


void					Render::loop(long start, long end)
{
	size_t				size = 0;
	std::queue<float*>	*bodies = new std::queue<float*>;
	t_frame_params		params;
	size_t				i = 0;

	this->tick = start;
	this->start_tick = start;
	
	//make our window
	SDL_Window *screen = SDL_CreateWindow("My Window",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          720, 680,
                          SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	SDL_GL_CreateContext(screen);
	SDL_GL_SwapWindow(screen);
	
	//I know I don't have to use a param struct for this but it feels cleaner
	params.size = &size;
	params.bodies = bodies;
	params.end = end;
	
	std::thread			thread(&Render::buffer_frames, this, &params);
	thread.detach();
	sleep(2); //give it some time to pre-buffer.
	while (1)
	{
		this->draw(size, bodies, screen, &i);
	}
	//the thread will never finish executing in the current implentation
	thread.join();
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
