#
# DEH_EDGE Plugin: < Linux MAKEFILE >
#

BIN=libdehedge.a

CXX=g++
AR=ar rc
RANLIB=ranlib

CXXFLAGS=-O -Wall
LDFLAGS=-Xlinker --warn-common
DEFINES=-DLINUX -DDEH_EDGE_PLUGIN

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
	$(AR) $(BIN) $(OBJS)
	$(RANLIB) $(BIN)

.PHONY: all clean

