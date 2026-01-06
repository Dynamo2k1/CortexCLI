CC = gcc
CFLAGS = -Wall -Werror -Wextra -pedantic
LIBS = -lcurl -ljansson -lreadline
NAME = dynamo

SRC = buildin.c checkbuild.c history.c line_exec.c linkpath.c shell.c string.c \
      ai_backend.c lang_detect.c safety.c audit.c
OBJ = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJ) $(LIBS)

%.o: %.c shell.h
	$(CC) $(CFLAGS) -c $< -o $@

# Quick build without intermediate .o files
quick:
	$(CC) $(CFLAGS) -o $(NAME) $(SRC) $(LIBS)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re quick
