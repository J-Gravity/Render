
#include "engine.h"

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
	Color::set_range(1.0, 4.0);
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

static inline unsigned char	h_avg(unsigned char a, unsigned char b)
{
	int						out;

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
}

int						thread_func(void *tmp)
{
	t_thread			*t;
	int					dif;
	Vector				*vec;
	double				mass;
	int					x, y, off;
	int					width, height, index;
	double				csize, cdif, ccoef, ccons;
	double				gcoef, gcons;

	t = (t_thread*)tmp;
	dif = (t->dad->width - t->dad->height) / 2;
	width = t->dad->width;
	height = t->dad->height;
	cdif = Color::max_mass - Color::min_mass;
	csize = Color::size;
	ccoef = csize / cdif;
	ccons = -csize * Color::min_mass / cdif;
	vec = new Vector();
	SDL_mutexP(t->dad->mutex);
	SDL_CondWait(t->dad->start_cond, t->dad->mutex);
	SDL_mutexV(t->dad->mutex);
	while (true)
	{
		gcoef = (double)width / (t->dad->scale * 3.0);
		gcons = (double)width * (t->dad->scale * 1.5) / (t->dad->scale * 3.0);
		for (long i = 0; i < t->count; i++)
		{
			if (t->dad->excl != 1 && i % t->dad->excl != 0)
			{
				fseek(t->f, 16, SEEK_CUR);
				continue;
			}
			h_set_vars(t->f, vec, mass);
			index = mass * ccoef + ccons;
			if (index >= csize)
				index = csize - 1;
			else if (index < 0)
				index = 0;
			t->dad->cam->watch_vector(vec);
			x = gcoef * vec->x + gcons;
			y = gcoef * vec->y + gcons - dif;
			if (x >= 0 && x < width && y >= 0 && y < height)
			{
				off = t->dad->width * y + x;
				if (t->compix[off] < index + 1)
					t->compix[off] = index + 1;
			}
		}
		fclose(t->f);
		SDL_mutexP(t->dad->mutex);
		t->dad->running_threads--;
		if (t->dad->running_threads == 0)
			SDL_CondSignal(t->dad->done_cond);
		SDL_CondWait(t->dad->start_cond, t->dad->mutex);
		SDL_mutexV(t->dad->mutex);
	}
	delete vec;
	return 0;
}

void					Render::init_threading()
{
	std::string			name;

	this->num_threads = 4;
	this->mutex = SDL_CreateMutex();
	this->start_cond = SDL_CreateCond();
	this->done_cond = SDL_CreateCond();
	this->threads = std::vector<t_thread*>(this->num_threads);
	this->tex = SDL_CreateTexture(this->ren,
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		this->width, this->height);
	this->pixels = std::vector<unsigned char>(this->width * this->height * 4,
		0);
	this->compix = std::vector<unsigned char>(this->width * this->height, 0);
	for (int i = 0; i < this->num_threads; i++)
	{
		this->threads[i] = new t_thread;
		this->threads[i]->id = i;
		this->threads[i]->dad = this;
		name = "thread_id" + std::to_string(i);
		this->threads[i]->compix = std::vector<unsigned char>(
			this->width * this->height, 0);
		SDL_CreateThread(thread_func, name.c_str(),
			(void*)this->threads[i]);
	}
}

void					Render::organize_threads(long par)
{
	std::string			name;
	long				cur;
	long				each;
	long				mod;

	cur = 8;
	each = par / this->num_threads;
	mod = par % this->num_threads;
	this->running_threads = this->num_threads;
	std::fill(this->pixels.begin(), this->pixels.end(), 0);
	std::fill(this->compix.begin(), this->compix.end(), 0);
	for (int i = 0; i < this->num_threads; i++)
	{
		name = this->in_path + std::to_string(this->tick) + ".jgrav";
		this->threads[i]->f = fopen(name.c_str(), "r");
		std::fill(this->threads[i]->compix.begin(), this->threads[i]->compix.end(), 0);
		fseek(this->threads[i]->f, cur, SEEK_SET);
		if (i + 1 == this->num_threads)
			each += mod;
		this->threads[i]->count = each;
		cur += each * 16;
	}
}

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
	name = this->out_path + std::to_string(this->tick) + ".jgpix";
	fout = fopen(name.c_str(), "w");
	fwrite((const char*)&this->compix[0], this->compix.size(),
		sizeof(char), fout);
	fclose(fout);
	SDL_RenderPresent(this->ren);
}

static void				h_update_from_input(Render *r, bool *u)
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
}

void					Render::loop(long start, long end)
{
	bool				running;
	bool				paused;
	bool				updated;
	bool				first;
	int					dir;
	SDL_Event			event;

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
			if (first)
			{
				this->draw(true);
				first = false;
			}
			else
				this->draw();
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
		h_update_from_input(this, &updated);
	}
	this->end_tick = this->tick;
}

void					Render::make_header(FILE *f)
{
	char head[] = {'G', 'I', 'F', '8', '9', 'a'};
	fwrite(head, sizeof(char), sizeof(head), f);
	Uint8 logical_sd[] = {
		this->width,
		this->width >> 8,
		this->height,
		this->height >> 8,
		0xF7,
		0x0, // Background index 1
		0x0 // Pixel aspect ratio
	};
	fwrite(logical_sd, sizeof(char), sizeof(logical_sd), f);
	char colors[3 * 256];
	for (int i = 1; i < 256; i++)
	{
		colors[3 * i + 0] = Color::table[i - 1]->r;
		colors[3 * i + 1] = Color::table[i - 1]->g;
		colors[3 * i + 2] = Color::table[i - 1]->b;
	}
	fwrite(colors, sizeof(char), sizeof(colors), f);
	fwrite("\x21\xff\x0bNETSCAPE2.0\x03\x01\x0\x0\0", 1, 19, f);
}

static unsigned char	*pack(std::vector<int> vec, int *len)
{
	unsigned char		*cur; // Current byte in data
	unsigned char		*out; // Location of data
	int					bits; // How many bits each number has
	int					obit; // How many bits have been put in byte
	int					ibit; // How many bits have been taken from number
	int					to_place; // Stores how many bits we are about to place
	unsigned char		masks[9] = {0, 1, 3, 7, 15, 31, 63, 127, 255};

	bits = 9;
	obit = 0;
	*len = 1;
	out = new unsigned char[vec.size() * 4]; // Allocate for worst case scenario
	*out = 0; // We are using |= so it needs to be zeroed
	cur = out;
	for (int i = 0; i < vec.size(); ++i) // Iterate through all numbers
	{
		ibit = 0;
		if (vec[i] == -1)
			bits++;
		else
		{
			while (ibit < bits)
			{
				to_place = ( (bits - ibit) < (8 - obit) ) ?
					(bits - ibit) : (8 - obit);
				*cur |= ( (vec[i] >> ibit) & masks[to_place] ) << obit;
				obit += to_place;
				ibit += to_place;
				if (obit > 7)
				{
					obit = 0;
					cur++;
					*cur = 0;
					(*len)++;
				}
			}
			if (vec[i] == 256)
				bits = 9;
		}
	}
	out[*len] = 0;
	return out;
}

static unsigned char	*encode(std::string &codee, int *len)
{
	int					dict_size = 258;
	int					bits = 9;
	std::map<std::string, int> dictionary;
	std::string			w;
	std::vector<int>	vec;

	for (int i = 0; i < 256; i++)
		dictionary[std::string(1, i)] = i;
	vec.push_back(256);
	for (std::string::const_iterator it = codee.begin();
		it != codee.end(); ++it)
	{
		char c = *it;
		std::string wc = w + c;
		if (dictionary.count(wc))
			w = wc;
		else
		{
			vec.push_back(dictionary[w]);
			dictionary[wc] = dict_size++;
			if ((dict_size - 1) >> bits != 0 && dict_size < 4095)
			{
				bits++;
				vec.push_back(-1);
			}
			if (dict_size >= 4096)
			{
				dictionary.clear();
				for (int i = 0; i < 256; i++)
					dictionary[std::string(1, i)] = i;
				dict_size = 258;
				vec.push_back(256);
				bits = 9;
			}
			w = std::string(1, c);
		}
	}
	if (!w.empty())
	{
		vec.push_back(dictionary[w]);
	}
	return pack(vec, len);
}

void					Render::make_frame(FILE *f, long tick)
{
	FILE				*in;
	std::string			name;
	char				*read_val;
	char				*pack;
	int					len;
	int					num_blocks;
	int					i;
	unsigned char		block_mod;

	Uint8 flags[] = {
		0x21,
		0xF9,
		0x04,
		0x04,
		0x04, // Frame length byte
		0x00, // And this one goes before it
		0x00, // No transparency
		0x00, // End of block
		0x2c, // We coloring now
		0x0,
		0x0,
		0x0,
		0x0,
		this->width,
		this->width >> 8,
		this->height,
		this->height >> 8,
		0x0, // No local color table, use global one
		0x08
	};
	fwrite(flags, sizeof(Uint8), sizeof(flags), f);
	name = this->out_path + std::to_string(tick) + ".jgpix";
	in = fopen(name.c_str(), "r");
	read_val = new char[this->width * this->height + 1];
	read_val[this->width * this->height] = 0;
	fread(read_val, sizeof(char), this->width * this->height, in);
	std::string			contents(read_val, this->width * this->height);
	delete read_val;
	pack = (char*)encode(contents, &len);
	num_blocks = (len) / 255;
	block_mod = (len) % 255;
	for (i = 0; i < num_blocks; ++i)
	{
		fwrite("\xff", 1, 1, f);
		fwrite(&(pack[i * 255]), 1, 255, f);
	}
	if (block_mod > 0)
	{
		fwrite(&block_mod, 1, 1, f);
		fwrite(&pack[i * 255], 1, block_mod, f);
	}
	fwrite("\x0", 1, 1, f);
	fclose(in);
	remove(name.c_str());
	delete pack;
}

void					Render::make_gif(std::string path)
{
	FILE				*f;

	f = fopen(path.c_str(), "w");
	this->make_header(f);
	if (this->start_tick >= 0 && this->end_tick > this->start_tick)
	{
		for (int i = this->start_tick; i <= this->end_tick; i++)
		{
			this->make_frame(f, i);
		}
	}
	fwrite("\x3b", 1, 1, f);
	fclose(f);
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
