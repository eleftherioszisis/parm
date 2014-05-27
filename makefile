UNAME := $(shell uname)
#CXX=${CXX}
SWIG=swig -Wextra -shadow -python -py3 -c++
CCOPTS=-Wall -O2 -fPIC #-std=c++11

#INC=-I/usr/include/python2.7
INC=`python3-config --includes`

#~ ifeq ("$(UNAME)","Darwin")
	#~ CC=/opt/local/bin/g++-mp-4.6
	#~ SWIG=/opt/local/bin/swig
	#~ INC=-I/opt/local/Library/Frameworks/Python.framework/Versions/Current/include/python2.7
	#~ LIB=/opt/local/lib/libpython2.7.dylib
#~ endif

.PHONY: all 2d 3d 2dlong 3dlong printout clean wraps

all: 2d 2dlong 3d 3dlong
	@echo "making all."

2d: lib/_sim2d.so

3d: lib/_sim3d.so

2dlong: lib/_sim2dlong.so

3dlong: lib/_sim3dlong.so

wraps: lib/sim_wrap2d.cxx lib/sim_wrap2dlong.cxx lib/sim_wrap3d.cxx lib/sim_wrap3dlong.cxx

printout:
	@echo Running on \"$(UNAME)\"

clean:
	cd lib; rm -f *.o *.so *.gch sim_wrap*.cxx

lib/sim_wrap2d.cxx: src/sim.i src/collection.hpp src/constraints.hpp src/interaction.hpp src/trackers.hpp src/box.hpp src/vecrand.hpp src/collection.cpp src/constraints.cpp src/interaction.cpp src/trackers.cpp src/box.cpp src/vecrand.cpp src/vec.hpp
	cd src ; $(SWIG) -DVEC2D sim.i
	mv src/sim_wrap.cxx lib/sim_wrap2d.cxx

lib/sim_wrap3d.cxx: src/sim.i src/collection.hpp src/constraints.hpp src/interaction.hpp src/trackers.hpp src/box.hpp src/vecrand.hpp src/collection.cpp src/constraints.cpp src/interaction.cpp src/trackers.cpp src/box.cpp src/vecrand.cpp src/vec.hpp
	cd src ; $(SWIG) -DVEC3D sim.i
	mv src/sim_wrap.cxx lib/sim_wrap3d.cxx

lib/sim_wrap2d.o: lib/sim_wrap2d.cxx
	$(CXX) $(CCOPTS) -DVEC2D -c lib/sim_wrap2d.cxx $(INC)

lib/sim_wrap3d.o: lib/sim_wrap3d.cxx
	$(CXX) $(CCOPTS) -DVEC3D -c lib/sim_wrap3d.cxx $(INC)

lib/_sim2d.so: lib/sim_wrap2d.o
	$(CXX) $(CCOPTS) -DVEC2D -shared lib/sim_wrap2d.o -o lib/_sim2d.so $(LIB)
	
lib/_sim.so: lib/sim_wrap3d.o
	$(CXX) $(CCOPTS) -DVEC3D -shared lib/sim_wrap3d.o -o lib/_sim.so $(LIB)

lib/sim_wrap2dlong.cxx: sim.i collection.hpp constraints.hpp interaction.hpp trackers.hpp box.hpp vecrand.hpp collection.cpp constraints.cpp interaction.cpp trackers.cpp box.cpp vecrand.cpp vec.hpp
	$(SWIG) -DVEC2D -DLONGFLOAT sim.i
	mv sim_wrap.cxx sim_wrap2dlong.cxx

lib/sim_wrap3dlong.cxx: sim.i collection.hpp constraints.hpp interaction.hpp trackers.hpp box.hpp vecrand.hpp collection.cpp constraints.cpp interaction.cpp trackers.cpp box.cpp vecrand.cpp vec.hpp
	$(SWIG) -DVEC3D -DLONGFLOAT sim.i
	mv sim_wrap.cxx sim_wrap3dlong.cxx

sim_wrap2dlong.o: sim_wrap2dlong.cxx
	$(CXX) $(CCOPTS)  -Wconversion -DVEC2D -DLONGFLOAT -c sim_wrap2dlong.cxx $(INC)

sim_wrap3dlong.o: sim_wrap3dlong.cxx
	$(CXX) $(CCOPTS) -Wconversion -DVEC3D -DLONGFLOAT -c sim_wrap3dlong.cxx $(INC)

_sim2dlong.so: sim_wrap2dlong.o
	$(CXX) $(CCOPTS) -Wconversion -DVEC2D -DLONGFLOAT -shared sim_wrap2dlong.o -o _sim2dlong.so $(LIB)
	
_sim3dlong.so: sim_wrap3dlong.o
	$(CXX) $(CCOPTS) -Wconversion -DVEC3D -DLONGFLOAT -shared sim_wrap3dlong.o -o _sim3dlong.so $(LIB)

VECOPTS := 2D 3D

define VEC_TARGET_RULE
vecrand$(1).o: vec.hpp vecrand.hpp vecrand.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c vecrand.cpp -o vecrand$(1).o

box$(1).o: vec.hpp vecrand.hpp box.hpp box.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c box.cpp -o box$(1).o

trackers$(1).o: vec.hpp vecrand.hpp box.hpp trackers.hpp trackers.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c trackers.cpp -o trackers$(1).o

interaction$(1).o: vec.hpp vecrand.hpp box.hpp trackers.hpp interaction.hpp interaction.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c interaction.cpp -o interaction$(1).o

constraints$(1).o: vec.hpp vecrand.hpp box.hpp trackers.hpp interaction.hpp constraints.hpp constraints.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c constraints.cpp -o constraints$(1).o
		
collection$(1).o: vec.hpp vecrand.hpp box.hpp trackers.hpp interaction.hpp collection.hpp constraints.hpp collection.cpp
	$(CXX) $(CCOPTS) -DVEC$(1) -c collection.cpp -o collection$(1).o

libsim$(1).so: vecrand$(1).o box$(1).o trackers$(1).o interaction$(1).o constraints$(1).o collection$(1).o 
	$(CXX) $(CCOPTS) -DVEC$(1) -shared -o libsim$(1).so box$(1).o trackers$(1).o vecrand$(1).o interaction$(1).o constraints$(1).o collection$(1).o

endef

$(foreach target,$(VECOPTS),$(eval $(call VEC_TARGET_RULE,$(target))))

LJatoms: libsim3D.so LJatoms.cpp
	$(CXX) $(CCOPTS) -DVEC3D LJatoms.cpp -L. -lsim3D -Wl,-rpath=. -o LJatoms

LJatoms2D: libsim2D.so LJatoms.cpp
	$(CXX) $(CCOPTS) -DVEC2D LJatoms.cpp -L. -lsim2D -Wl,-rpath=. -o LJatoms2D

hardspheres: libsim3D.so hardspheres.cpp
	$(CXX) $(CCOPTS) -DVEC3D hardspheres.cpp -L. -lsim3D -Wl,-rpath=. -o hardspheres

basicsim.zip: box.cpp box.hpp collection.cpp collection.hpp constraints.cpp constraints.hpp hardspheres.cpp interaction.cpp interaction.hpp LJatoms.cpp makefile trackers.cpp trackers.hpp vec.hpp vecrand.cpp vecrand.hpp .vmdrc
	zip -r $@ $^
