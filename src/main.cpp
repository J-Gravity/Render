
#include "engine.h"

Render				*g_render;

int					main(int argc, char *argv[])
{
	std::string		path;
	if (argc < 3)
	{
		std::cerr << "Usage: ./grav [prefix] [start frame] [end frame]\n";
		std::cerr << "Example: ./grav big_data/multimass- 0 1199\n";
		return 0;
	}
	path = argv[1];
	g_render = new Render(1600, 1200, path);
	g_render->loop(atoi(argv[2]), atoi(argv[3]));
	g_render->wait_for_death();
	return (0);
}
