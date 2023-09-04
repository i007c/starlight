
SOURCES = $(shell find src -type f -name "*.c" -not -path "*/.*")
OBJECTS = $(addprefix build/, $(SOURCES:.c=.o))
LIBNAME = ~/.local/lib/libstarlight
HEADER = starlight.h

CC = gcc
CFLAGS  = -std=c11 -O0 -g -pedantic -Wall -Wextra -Wpedantic -Werror
CFLAGS += -I. -D_GNU_SOURCE


shared: CFLAGS += -fpic
shared: clear $(OBJECTS)
	$(CC) -shared -o $(LIBNAME).so $(OBJECTS)


static: clear $(OBJECTS)
	ar rcs $(LIBNAME).a $(OBJECTS)


build/%.o: %.c $(HEADER)
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $< -o $@


clean:
	rm -rf build $(LIBNAME).*


clear:
	printf "\E[H\E[3J"
	clear

.PHONY: clear clean shared static
.SILENT: clear clean $(HEADER) shared static

