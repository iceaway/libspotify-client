CC = gcc
TARGET = cmd
LIBSPOTIFY = /usr/local/lib
INC_SPOTIFY = /usr/local/include

CFLAGS += -Wall -Wextra -pedantic -O0 -g3 -I${INC_SPOTIFY}
LDFLAGS += -L${LIBSPOTIFY}
LDLIBS += -lspotify
LDLIBS += -lpthread
LDLIBS += -lreadline
LDLIBS += -lasound

OBJS += cmd.o
OBJS += session.o
OBJS += appkey.o
OBJS += audio.o 

${TARGET}: ${OBJS}
	${CC} ${CFLAGS} ${LDLIBS} ${LDFLAGS} ${OBJS} -o ${TARGET}
	

clean:
	rm -f ${TARGET} ${OBJS}
