
#include "engine.h"

Render				*g_render;

int					main(int argc, char *argv[])
{
	std::string		path;
	if (argc < 5)
	{
		std::cerr << "Usage: ./grav [prefix] [start frame] [end frame] [exclusion]\n";
		std::cerr << "Example: ./grav big_data/multimass- 0 1199 1\n";
		return 0;
	}
	path = argv[1];
	g_render = new Render(1200, 900, path, "output/o-", atoi(argv[4]));
	g_render->loop(atoi(argv[2]), atoi(argv[3]));
	return (0);
}
