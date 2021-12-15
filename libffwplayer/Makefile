
MEDIA_TEST_FILE = ~/Videos/ironman.mp4

EXEC = ffwplayer

OBJS = ffwplayer.o \
       log.o \
       msg_thread.o

CFLAGS = -g `sdl2-config --cflags`

LINK_FLAGS = -pthread \
              -lavutil \
              -lavformat \
              -lavcodec \
              -lswscale \
              -lswresample \
              -lz \
              -lm \
              `sdl2-config --libs`

all: ${EXEC}

ffwplayer.o: ffwplayer.h log.h msg_thread.h
	gcc ffwplayer.c -c -o ffwplayer.o ${CGLAGS}

log.o: log.h
	gcc log.c -c -o log.o ${CGLAGS}

msg_thread.o: ffwplayer.h log.h
	gcc msg_thread.c -c -o msg_thread.o ${CGLAGS}

${EXEC}: ${OBJS}
	gcc ${OBJS} -o ${EXEC} ${LINK_FLAGS}

clean:
	rm *.o
	rm ${EXEC}

test: ${EXEC}
	./${EXEC} ${MEDIA_TEST_FILE}
