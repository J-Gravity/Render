
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
	g++ -L /usr/local/lib -std=c++11 -lGL -lGLU -lglut -pthread src/main.cpp src/render.cpp -I /usr/local/includes -I includes/ -lSDL2 -o render

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
