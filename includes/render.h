
#ifndef RENDER_H
# define RENDER_H

# include "engine.h"
# include <cstdlib>
# include <fcntl.h>
# include <unistd.h>
# include <stdio.h>
# include <SDL.h>
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <queue>

# define FRAME_COUNT 36

class				Render;

typedef struct					s_thread
{
	int							id;
	FILE						*f; // Already at the correct cursor location
	long						count; // In number of particles
	Render						*dad;
	std::vector<unsigned char>	compix;
}								t_thread;

class				Render
{
public:
	int							width;
	int							height;
	SDL_Window					*win;
	SDL_Renderer				*ren;
	Camera						*cam;
	long						tick;
	double						scale;
	std::string					in_path;
	std::string					out_path;
	Uint8						*keystate;
	SDL_mutex					*mutex;
	SDL_cond					*start_cond;
	SDL_cond					*done_cond;
	int							num_threads;
	int							running_threads;
	int							excl;
	SDL_Texture					*tex;
	std::vector<unsigned char>	pixels;
	std::vector<unsigned char>	compix;
	bool						to_gif;

	Render(int w = 640, int h = 480, std::string in = "data/test-",
		std::string out = "output/o-", int excl = 1);
	void						draw(bool first = false);
	void						loop(long start, long end);
	void						make_gif(std::string path = "out.gif");
	void						wait_for_death();
	~Render();

private:
	std::vector<t_thread*>		threads;
	long						start_tick;
	long						end_tick;

	void						make_header(FILE *f);
	void						make_frame(FILE *f, long tick);
	void						init_threading();
	void						organize_threads(long par);
	
	//my shit
	void						draw(size_t size, float *bodies, SDL_Window *screen);
	void						draw(size_t size, std::queue<float*> *bodies, SDL_Window *screen, size_t *i);
	void						buffer_frames(void *p);
	float						*getBodies(int fd, size_t size, float *ret);
};

#endif
