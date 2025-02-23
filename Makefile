# Dumb mini makefile: assumes all .c files (except common.c) map to an executable
# and all targets depend on all .h files (so no dep building)

# if CC is the default (not environment varible nor supplied to make, then default to total hack)
ifeq ($(origin CC),default)
  CC = clang-15
endif

# this is janky
CFLAGS ?= -g -O3 ${IDIRS} -march=native -Wall -Wextra -Wconversion -Wpedantic -Wno-unused-function

LDLIBS  = -lmylib -ltestu01 -lm
IDIRS   = -Iextern

SRC     := ${filter-out common.c, ${wildcard *.c}}
HEADERS := ${wildcard *.h}
TARGETS := ${SRC:.c=}

all:    ${TARGETS}

clean:
	-${RM} ${TARGETS}

distclean:	clean

# needs TestU01
mini_testu01:mini_testu01.c	Makefile common.c ${HEADERS}
	${CC} ${CFLAGS} common.c $< -o $@ ${LDLIBS}

%:%.c	Makefile common.c ${HEADERS}
	${CC} ${CFLAGS} common.c $< -o $@

