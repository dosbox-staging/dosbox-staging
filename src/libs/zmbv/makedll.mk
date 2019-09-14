CPP=g++
CPPFLAGS=-O3
LIBS=-static -lz -lwinmm -static-libgcc -static-libstdc++

OBJ = resource.o zmbv.o drvproc.o zmbv_vfw.o

default: zmbv.dll


resource.o: resource.rc
	windres -i resource.rc -o resource.o

%.o: %.cpp
	$(CPP) -c -o $@ $< $(CPPFLAGS)

zmbv.dll: $(OBJ) 
	$(CPP) -shared -o $@ $^ $(CPPFLAGS) $(LIBS) zmbv_mingw.def