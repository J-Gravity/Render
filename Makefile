
NAME	= grav

SRC		= main.cpp render.cpp
OBJ		= $(addprefix $(OBJDIR),$(SRC:.cpp=.o))

CC		= clang++
CGLAGS	= -std=c++11 -pthread -O3 -I /usr/local/include/sdl -L /usr/local/lib
LDFLAGS	= -std=c++11 -pthread -I /usr/local/include/sdl -L usr/local/lib

SRCDIR	= ./src/
INCDIR	= ./includes/
OBJDIR	= ./obj/

all: obj
	clang++ -o render src/render.cpp src/main.cpp `sdl2-config --cflags --libs` -I includes -pthread -std=c++11

obj:
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o: $(SRCDIR)%.cpp
	$(CC) $(CGLAGS) -I $(INCDIR) -o $@ -c $<

$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -rf $(NAME)

f: re
	./$(NAME)

re: fclean all
