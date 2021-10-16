#
# DEH_EDGE Plugin: < MacOS X MAKEFILE >
#

BIN=libdehedge.a

CXX=g++

CXXFLAGS=-O -Wall
LDFLAGS=
DEFINES=-DMACOSX -DDEH_EDGE_PLUGIN

OBJS=convert.o main.o system.o util.o wad.o info.o mobj.o \
     sounds.o frames.o things.o attacks.o weapons.o ammo.o \
	 misc.o text.o storage.o patch.o rscript.o buffer.o

# ----- TARGETS ------------------------------------------------------

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJS) ERRS core core.* deh_debug.log

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

$(BIN): $(OBJS)
	libtool -static -o $(BIN) - $(OBJS)
	ranlib $(BIN)

.PHONY: all clean

