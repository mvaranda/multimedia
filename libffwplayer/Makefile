
MEDIA_TEST_FILE = ~/Videos/ironman.mp4

EXEC = ffwplayer

OBJS = ffwplayer.o \
       log.o \
       msg_thread.o

CFLAGS = -g  -D TEST_FFWPLAYER_LIBRARY `sdl2-config --cflags`

LINK_FLAGS = -g -pthread \
              -lavutil \
              -lavformat \
              -lavcodec \
              -lswscale \
              -lswresample \
              -lz \
              -lm \
              `sdl2-config --libs`

all: ${EXEC}

ffwplayer.o: ffwplayer.c ffwplayer.h log.h msg_thread.h
	gcc ffwplayer.c -c -o ffwplayer.o $(CFLAGS)

log.o: log.c log.h
	gcc log.c -c -o log.o ${CFLAGS}

msg_thread.o: msg_thread.c ffwplayer.h log.h
	gcc msg_thread.c -c -o msg_thread.o ${CFLAGS}

${EXEC}: ${OBJS}
	gcc ${OBJS} -o ${EXEC} ${LINK_FLAGS}

clean:
	rm *.o
	rm ${EXEC}

test: ${EXEC}
	./${EXEC} ${MEDIA_TEST_FILE}
