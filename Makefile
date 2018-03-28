
CC = gcc

Cflag = -g -O2 -Wall -fmessage-length=0 -Wfatal-errors

Path = "/opt/contrib/ffmpeg/lib"

INCLUDE = -I"/opt/contrib/ffmpeg/include"

SRC = $(wildcard *.c)

OBJS = $(patsubst %c, %o, $(SRC))

Lib = -lavdevice -lavfilter -lavformat -lavcodec -lpostproc -lswresample -lswscale -lavutil

Lib +=	$(Path)/libavformat.a
Lib +=	$(Path)/libswresample.a
Lib +=	$(Path)/libswscale.a
Lib +=	$(Path)/libavfilter.a
Lib +=	$(Path)/libavcodec.a
Lib +=	$(Path)/libavutil.a
Lib +=	/opt/contrib/x264/lib/libx264.a
Lib +=	/opt/contrib/fdk_aac/lib/libfdk-aac.a
Lib +=  /opt/contrib/freetype2/lib/libfreetype.a
Lib += -lmp3lame 
Lib += -lopus 
Lib += -lass
Lib += -ltheora 
Lib += -ltheoradec 
Lib += -ltheoraenc 
Lib += -lvorbis 
Lib += -lvorbisenc
Lib += -lvorbisfile
Lib += -lvpx 
Lib += -lva-drm
Lib += -lva-egl
Lib += -lva-glx
Lib += -lva-tpi
Lib += -lva-x11
Lib += -lva
Lib += -lXv -lX11 -lXext -ldl -ldl -lvdpau -lva -lva-x11 -lX11 -lva -lva-drm -lva -lxcb -lXau -lXdmcp -lxcb-shm -lxcb -lXau -lXdmcp -lxcb-xfixes -lxcb-render -lxcb-shape -lxcb -lXau -lXdmcp -lxcb-shape -lxcb -lXau -lXdmcp -lasound -Wl,--no-undefined -lSDL2 -lasound -lm -ldl -lpulse-simple -lpulse -lX11 -lXext -lXcursor -lXinerama -lXi -lXrandr -lXss -lXxf86vm -lwayland-egl -lwayland-client -lwayland-cursor -lxkbcommon -lts -lpthread -lrt -lx264 -lpthread -lm -ldl -lfreetype -lz -lpng12 -lfdk-aac -lm -lm -lbz2 -lz -pthread 
LIBS += -lpthread -lrt -ldl -lz -lbz2

Target = mosaic

all: $(Target)

%.o:%.c
	$(CC) $(Cflag) -c $< -o $@ $(INCLUDE)

$(Target):$(OBJS)
	$(CC) $(Cflag) -o $@ $^  -L$(Path)  $(Lib) $(Lib2) 

.PHONY:clean

clean:
	@rm -f $(OBJS) $(Target)