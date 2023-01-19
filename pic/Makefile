CC = gcc --std=c99

OBJS = pic.o xpic.o ppm.o adaptcm.o jpeg.o

LIB = libpicio.a

$(LIB): $(OBJS)
	ar cr $(LIB) $(OBJS)

clean:
	/bin/rm -f $(LIB) $(OBJS)
