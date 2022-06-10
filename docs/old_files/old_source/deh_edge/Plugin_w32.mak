#
# DEH_EDGE Plugin: < Win32 + MinGW MAKEFILE >
#
# (Note: requires GNU make)
# 

# SHELL = bash.exe

CXX=g++.exe
AR=ar r
RANLIB=ranlib

BIN=libdehedge.a

## # External library directories
## DEVCPP_DIR = "c:/localbin/devcpp"
## 
## INCS =  -I$(DEVCPP_DIR)/include  -Iinclude
## 
## CXXINCS = -I$(DEVCPP_DIR)/include/c++ \
##           -I$(DEVCPP_DIR)/include/c++/mingw32 \
##           -I$(DEVCPP_DIR)/include/c++/backward \
## 		  -I$(DEVCPP_DIR)/lib/gcc-lib/mingw32/3.2/include
## 
## CXXFLAGS = $(CXXINCS) $(INCS) -DWIN32 -DDEH_EDGE_PLUGIN -O1
           
CXXFLAGS=-O1 -Wall
LDFLAGS=
DEFINES=-DWIN32 -DDEH_EDGE_PLUGIN

OBJS=convert.o main.o system.o util.o wad.o info.o mobj.o \
     sounds.o frames.o things.o attacks.o weapons.o ammo.o \
	 misc.o text.o storage.o patch.o rscript.o buffer.o
    
# ----- TARGETS ------------------------------------------------------

all: $(BIN)

clean:
	rm -f $(OBJS) $(BIN) 
	
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

$(BIN): $(OBJS)
	$(AR) $(BIN) $(OBJS)
	$(RANLIB) $(BIN)
 
.PHONY: all clean

