
NAME	= grav

SRC		= main.cpp render.cpp
OBJ		= $(addprefix $(OBJDIR),$(SRC:.cpp=.o))

CC		= clang++
CGLAGS	= -std=c++11 -pthread -O3 -I /usr/local/include/sdl -L /usr/local/lib
LDFLAGS	= -Wl -rpath ./frameworks/ -F ./frameworks/ -framework SDL2

SRCDIR	= ./src/
INCDIR	= ./includes/
OBJDIR	= ./obj/

all: obj
	g++ -std=c++11 -pthread src/main.cpp src/render.cpp -I /usr/local/includes -I frameworks/SDL2.framework/Headers/ -I includes/ -o render $(LDFLAGS)

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
