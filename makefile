# In short: this is very close to supporting Python 2, but supporting 2 and 3 at
# the same time was too much for me and my `makefile`.

# Most of this library is Python-version agnostic, except for the `makefile`,
# which includes `-py3` as a swig option by default. I did not want to remove
# this, as it adds useful functionality, but I also could not find a good way to
# include it only when Python 3 is wanted.

# I tried using a setup.py that invokes swig, but that was even more difficult.

UNAME := $(shell uname)
#CXX=${CXX}
SWIG=swig -Wextra -shadow -python -py3 -c++
CCOPTS=-I src -Wall -O2 -fPIC -std=c++98

INC=`python3-config --includes`
LIB=`python3-config --ldflags`

.PHONY: all py2d py3d py2dlong py3dlong printout clean wraps py wrap2d wrap3d wrap2dlong wrap3dlong doc ghp

all: py2d py3d
	@echo "making 2d and 3d."

py: py2d py3d py2dlong py3dlong


wraps: wrap2d wrap3d wrap2dlong wrap3dlong

printout:
	@echo Running \"$(CXX)\" on \"$(UNAME)\"
	
ghp: doc
	echo 'parm.lostinmyterminal.com' > doc/html/CNAME
	ghp-import doc/html
	
doc: doc/html/index.html
	
doc/html/index.html: src/*.cpp src/*.hpp src/*.md src/bin/*.cpp pyparm/examples/*.py Doxyfile README.md
	doxygen Doxyfile >/dev/null
	
clean:
	rm -f bin/* lib/*
	cd src; rm -f *.o *.so *.gch
	cd pyparm; rm -f *.o *.so *.gch
	rm -f src/sim.py

lib:
	mkdir -p lib

bin:
	mkdir -p bin

# We now define the four options: (2d or 3d) + (doubles or long doubles)
# SFX is the suffix for object files, and MODNAME is the module name for the python modules
# OPTSET is the appropriate option set for each version

VECOPTS := 2 3

FLOATOPTS := long notlong

define TARGET_RULES

$(eval NDIM=$(1))
$(if $(findstring notlong,$(2)), $(eval FLT=), $(eval FLT=long))
$(if $(findstring notlong,$(2)), $(eval FLTOPT=), $(eval FLTOPT=-DLONGFLOAT))

$(eval OPTSET=-DVEC$(NDIM)D $(FLTOPT))
$(eval SFX:=$(NDIM)d$(FLT))
$(eval MODNAME:=d$(NDIM)$(FLT))

#-------------------------------------------------------------------------------
# The python modules

py$(SFX): pyparm/_sim$(SFX).so

# We use -DSWIG_TYPE_TABLE=sim$(SFX) so that types from sim2d and sim3d with the
# same names do not end up treated as the same types. This can cause issues with
# e.g. AtomVec, if both sim2d and sim3d are imported at the same time.
# There isn't any need for the two to interoperate, so we keep them separate.
wrap$(SFX):
	cd src ; $(SWIG) $(OPTSET) -DSWIG_TYPE_TABLE=sim$(SFX) sim.i
	(cat src/swig_header.h ; echo ; echo ; cat src/sim_wrap.cxx) > pyparm/sim_wrap$(SFX).cxx
	rm src/sim_wrap.cxx
	mv src/sim$(SFX).py pyparm/$(MODNAME).py

pyparm/sim_wrap$(SFX).o: pyparm/sim_wrap$(SFX).cxx
	$(CXX) $(CCOPTS) $(OPTSET) -DSWIG_TYPE_TABLE=sim$(SFX) -I src/ -c pyparm/sim_wrap$(SFX).cxx -o pyparm/sim_wrap$(SFX).o $(INC)

pyparm/_sim$(SFX).so: pyparm/sim_wrap$(SFX).o
	$(CXX) $(CCOPTS) $(OPTSET) -shared pyparm/sim_wrap$(SFX).o -o pyparm/_sim$(SFX).so $(LIB)

#-------------------------------------------------------------------------------
# The C++ modules
lib/vecrand$(SFX).o: src/vecrand.hpp src/vecrand.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/vecrand.cpp -o lib/vecrand$(SFX).o

lib/box$(SFX).o: src/vecrand.hpp src/box.hpp src/box.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/box.cpp -o lib/box$(SFX).o

lib/trackers$(SFX).o: src/vecrand.hpp src/box.hpp src/trackers.hpp src/trackers.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/trackers.cpp -o lib/trackers$(SFX).o

lib/interaction$(SFX).o: src/vecrand.hpp src/box.hpp src/trackers.hpp src/interaction.hpp src/interaction.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/interaction.cpp -o lib/interaction$(SFX).o

lib/constraints$(SFX).o: src/vecrand.hpp src/box.hpp src/trackers.hpp src/interaction.hpp src/constraints.hpp src/constraints.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/constraints.cpp -o lib/constraints$(SFX).o

lib/collection$(SFX).o: src/vecrand.hpp src/box.hpp src/trackers.hpp src/interaction.hpp src/collection.hpp src/constraints.hpp src/collection.cpp | lib
	$(CXX) $(CCOPTS) $(OPTSET) -c src/collection.cpp -o lib/collection$(SFX).o

lib/libsim$(SFX).so: lib/vecrand$(SFX).o lib/box$(SFX).o lib/trackers$(SFX).o lib/interaction$(SFX).o lib/constraints$(SFX).o lib/collection$(SFX).o | lib
	$(CXX) $(CCOPTS) $(OPTSET) -shared -o lib/libsim$(SFX).so lib/box$(SFX).o lib/trackers$(SFX).o lib/vecrand$(SFX).o lib/interaction$(SFX).o lib/constraints$(SFX).o lib/collection$(SFX).o

bin/LJatoms$(SFX): lib/libsim$(SFX).so src/bin/LJatoms.cpp | bin
	$(CXX) $(CCOPTS) $(OPTSET) src/bin/LJatoms.cpp -Llib -lsim$(SFX) -Wl,-rpath "lib" -o bin/LJatoms$(SFX)

bin/packer$(SFX): lib/libsim$(SFX).so src/bin/packer.cpp | bin
	$(CXX) $(CCOPTS) $(OPTSET) src/bin/packer.cpp -Llib -lsim$(SFX) -Wl,-rpath "lib" -o bin/packer$(SFX)

endef

$(foreach target1,$(VECOPTS), $(foreach target2,$(FLOATOPTS),$(eval $(call TARGET_RULES,$(target1),$(target2)))))

bin/hardspheres: lib/libsim3d.so src/bin/hardspheres.cpp | bin
	$(CXX) $(CCOPTS) -DVEC3D src/bin/hardspheres.cpp -Llib -lsim3d -Wl,-rpath "lib" -o bin/hardspheres

bin/hardspheres2: lib/libsim3d.so src/bin/hardspheres2.cpp | bin
	$(CXX) $(CCOPTS) -DVEC3D src/bin/hardspheres2.cpp -Llib -lsim3d -Wl,-rpath "lib" -o bin/hardspheres2
