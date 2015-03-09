#include "vecrand.hpp"

#ifndef BOX_H
#define BOX_H

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>

#define sptr boost::shared_ptr

using namespace boost; // required for SWIG for some reason

typedef const unsigned int cuint;

inline bool toBuffer(vector<Vec*> arr, double* buffer, size_t sizet) {
    if(sizet < NDIM * arr.size()){return false;};
    for(uint i=0; i < arr.size(); i++)
    for(uint j=0; j < NDIM; j++)
    {
        buffer[i*NDIM + j] = (double) ((*arr[i])[j]);
    }
    return true;
};

class atomgroup;

/*!
@defgroup basics Basics

The basic classes that almost all simulations will need.
*/

/*!
@defgroup boxes Boxes

Classes and functions related to the boundary conditions of the simulation.
*/

/*!
@defgroup atoms Atoms

Particle related functions.
*/


/*!
@ingroup basics boxes
@brief The virtual interface for the shape of the space and its boundaries.
*/
class Box {
    public:
        //! Distance between two points, given boundary conditions.
        /*!
        This is the main function that Box exists for.
        */
        virtual Vec diff(Vec r1, Vec r2)=0;
        //! Volume. Can return NaN.
        virtual flt V()=0;
        virtual ~Box(){};
};

/***********************************************************************
 * Boxes
 */

//! The modulus function for Vec
/*!
@ingroup boxes
*/
#ifdef VEC3D
inline Vec vecmod(Vec r1, Vec r2){
    return Vec(remainder(r1[0], r2[0]), remainder(r1[1], r2[1]), remainder(r1[2], r2[2]));
};
#endif
#ifdef VEC2D
inline Vec vecmod(Vec r1, Vec r2){
    return Vec(remainder(r1[0], r2[0]), remainder(r1[1], r2[1]));
};
#endif

//! An infinite Box, for use with, e.g., sticky conglomerations or proteins.
/*!
@ingroup basics
@ingroup boxes
*/
class InfiniteBox : public Box {
    public:
        //! Simply `r1-r2`.
        Vec diff(Vec r1, Vec r2){return r1-r2;};
        //! Returns NaN.
        flt V(){return NAN;};
};

//! A rectilinear Box, with periodic boundary conditions.
/*!
@ingroup basics
*/
class OriginBox : public Box {
    protected:
        Vec boxsize;
    public:
        OriginBox(Vec size) : boxsize(size){};
        Vec diff(Vec r1, Vec r2){
            return vecmod((r1-r2), boxsize);
        };
        virtual Vec diff(Vec r1, Vec r2, array<int,NDIM> boxes){
            Vec dr = r1 - r2;
            for(uint i=0; i<NDIM; i++) dr[i] -= boxsize[i] * boxes[i];
            return dr;
        };
        //! The `div` function to go with `diff`.
        virtual array<int,NDIM> box_round(Vec r1, Vec r2){
            array<int,NDIM> boxes;
            Vec dr = r1 - r2;
            for(uint i=0; i<NDIM; i++) boxes[i] = (int) round(dr[i] / boxsize[i]);
            return boxes;
        };
        #ifdef VEC3D
        OriginBox(flt L) : boxsize(L,L,L){};
        flt V(){return boxsize[0] * boxsize[1] * boxsize[2];};
        flt L(){return (boxsize[0] + boxsize[1] + boxsize[2])/3.0;};
        #endif
        #ifdef VEC2D
        OriginBox(flt L) : boxsize(L,L){};
        flt V(){return boxsize[0] * boxsize[1];};
        flt L(){return (boxsize[0] + boxsize[1])/2.0;};
        #endif
        //! Resize by a factor. Does not move atoms.
        flt resize(flt factor){boxsize *= factor; return V();}
        //! Resize to a specific shape.
        flt resize(Vec newsize){boxsize = newsize; return V();}
        //! Resize to a specific volume.
        flt resizeV(flt newV){
            flt curV = V();
            boxsize *= pow(newV/curV, OVERNDIM);
            return V();
        }
        //! Resize to a specific volume.
        flt resizeL(flt newL){
            flt curL = pow(V(), OVERNDIM);
            boxsize *= newL/curL;
            return V();
        }
        //! Get a random point in the box.
        Vec randLoc(){
            Vec v = randVecBoxed();
            for(uint i=0; i<NDIM; i++){
                v[i] *= boxsize[i];
            }
            return diff(v, Vec());
        };
        Vec boxshape(){return boxsize;};
};

//! Lees-Edwards boundary conditions, with shear in the x-direction, relative to y.
/*!
@ingroup boxes
*/
class LeesEdwardsBox : public OriginBox {
    protected:
        flt gamma;
    public:
        LeesEdwardsBox(Vec size, flt gamma=0.0) : OriginBox(size), gamma(gamma){};
        Vec diff(Vec r1, Vec r2);
        virtual Vec diff(Vec r1, Vec r2, array<int,NDIM> boxes);
        virtual array<int,NDIM> box_round(Vec r1, Vec r2);

        //! The current shear amount.
        flt get_gamma(){return gamma;};

        //! Change the shear by dgamma, and move the atoms as necessary.
        void shear(flt dgamma, atomgroup &atoms);
        Vec nonaffine(Vec v){
            v[0] -= gamma * v[1];
            return v;
        }

        Vec affine(Vec v){
            v[0] += gamma * v[1];
            return v;
        }
};

//! A spheocylinder box, also known as a capsule.
/*!
@ingroup boxes
Note that this *does not* keep particles inside the box; use an interaction like SCboxed for that.

This class is useful for functions like V(), dist, edgedist, inside, randLoc, etc.

The spherocylinder has an axis along the x-axis, centered at origin

L is length of the central axis of the cylinder from 1 sphere center to the other, so L=0 is a sphere.
*/
class SCbox : public Box {
    protected:
        flt L, R;
    public:
        SCbox(flt L, flt R);
        Vec diff(Vec r1, Vec r2){return r1-r2;};
        flt V();
        Vec dist(Vec r1);
        Vec edgedist(Vec r1);
        bool inside(Vec r1, flt buffer=0.0);
        Vec randLoc(flt min_dist_to_wall=0.0);
        flt length(){return L;};
        flt radius(){return R;};
};

////////////////////////////////////////////////////////////////////////////////
//! The basic class for representing each particle.
/*!
@ingroup atoms
Normally instantiated through atomvec.
*/
struct atom {
    //! location.
    Vec x;

    //! velocity
    Vec v;

    //! acceleration
    Vec a;

    //! forces
    Vec f;

    //! mass
    flt m;
};

//! A pointer to an atom.
class atomref {
    private:
        atom *ptr;
    public:
        inline atomref() : ptr(NULL){};
        inline atomref(atom *a) : ptr(a){};
        inline atom& operator *() const {return *ptr;};
        inline atom *operator->() const{ return ptr;}
        inline bool operator==(const atomref &other) const {return other.ptr == ptr;};
        inline bool operator==(const atom* other) const {return other == ptr;};
        inline bool operator!=(const atomref &other) const {return other.ptr != ptr;};
        inline bool operator<(const atomref &other) const {return ptr < other.ptr;};
        inline bool operator<=(const atomref &other) const {return ptr <= other.ptr;};
        inline bool operator>=(const atomref &other) const {return ptr >= other.ptr;};
        inline bool operator>(const atomref &other) const {return ptr > other.ptr;};
        inline bool is_null(){return ptr==NULL;};
};

//! A pointer to an atom, that also knows its own index in an atomvec.
/*!
@ingroup atoms
This is used in many interaction classes to compair atoms.
*/
class atomid : public atomref {
    private:
        uint num; // note that these are generally only in reference to
                  // a specific atomgroup
    public:
        inline atomid() : atomref(), num(UINT_MAX){};
        // inline atomid(atom *a) : atomref(a), num(UINT_MAX){};
        inline atomid(atom *a, uint n) : atomref(a), num(n){};
        inline uint n() const {return num;};
};

class idpair : public Array<atomid, 2> {
    public:
        idpair(){vals[0] = atomid(); vals[1] = atomid();};
        idpair(atomid a, atomid b){ vals[0] = a; vals[1] = b;};
        inline atomid first() const {return vals[0];};
        inline atomid last() const {return vals[1];};
};

class atomgroup;

//! For iterating through an atomgroup.
class AtomIter{
    private:
        uint i;
        atomgroup &g;
    public:
        AtomIter (atomgroup& g, uint i): i(i), g(g){};
        bool operator!=(const AtomIter& other) const{return i != other.i;};
        atom& operator* () const;
        inline const AtomIter& operator++(){++i; return *this;};
};

class atomvec;

//! a group of atoms, such as all of them (atomvec), or a smaller group such as a molecule, sidebranch, etc.
/*!
@ingroup atoms
*/
class atomgroup {
    public:
        // access individual atoms
        virtual atomvec& vec()=0;
        virtual atom& operator[](cuint n)=0;
        virtual atom& operator[](cuint n) const=0;
        virtual atom& get(cuint n){return ((*this)[n]);};
        virtual atomid get_id(cuint n)=0;
        //! Number of atoms in the group
        virtual uint size() const=0;
        //! For use in a for loop
        virtual AtomIter begin(){return AtomIter(*this, 0);};
        virtual AtomIter end(){return AtomIter(*this, (uint) size());};

        //! center of mass
        Vec com() const;
        //!center of mass velocity
        Vec comv() const;

        //! Mass of the whole group
        flt mass() const;
        //! Total kinetic energy of the group.
        /*!
        This is normally with reference to a "lab" reference frame (velocity (0,0,0)), but
        can optionally take a different origin velocity, e.g. `comv()`.
        */
        flt kinetic(const Vec &originvelocity=Vec()) const;
        //! Total momentum.
        Vec momentum() const;
        //! \f$R_g\f$
        flt gyradius() const;
        #ifdef VEC3D
        flt moment(const Vec &loc, const Vec &axis, Box &box) const;
        Vec angmomentum(const Vec &loc, Box &box) const;
        Matrix<flt> moment(const Vec &loc, Box &box) const;
        Vec omega(const Vec &loc, Box &box) const;
        void addOmega(Vec w, Vec origin, Box &box);
        inline void resetL(Box &box){
            Vec c = com(), w = omega(c, box);
            if (w.sq() == 0) return;
            addOmega(-w, c, box);
        }
        #elif defined VEC2D
        flt moment(const Vec &loc, Box &box) const;
        flt angmomentum(const Vec &loc, Box &box) const;
        flt omega(const Vec &loc, Box &box) const{return angmomentum(loc, box) / moment(loc, box);};
        void addOmega(flt w, Vec origin, Box &box);
        inline void resetL(Box &box){
            Vec c = com();
            flt w = omega(c, box);
            if (w == 0) return;
            addOmega(-w, c, box);
        }
        #endif

        //! for resetting
        void addv(Vec v);
        void resetcomv(){addv(-comv());};
        void randomize_velocities(flt T);

        //! for timestepping
        void resetForces();
        virtual ~atomgroup(){};
};

inline atom& AtomIter::operator*() const{return g[i];};

/*!
@ingroup basics atoms
@brief The main class for representing particles.
*/
class atomvec : public virtual atomgroup {
    private:
        atom* atoms;
        uint sz;
    public:
        atomvec(vector<double> masses) : sz((uint) masses.size()){
            atoms = new atom[sz];
            for(uint i=0; i < sz; i++) atoms[i].m = masses[i];
        };
        atomvec(uint N, flt mass) : sz(N){
            atoms = new atom[sz];
            for(uint i=0; i < sz; i++) atoms[i].m = mass;
        };
        atomvec(atomvec& other) : sz(other.size()){
            atoms = new atom[sz];
            for(uint i=0; i < sz; i++) atoms[i] = other.atoms[i];
        };
        atomvec& vec(){return *this;};
        inline atom& operator[](cuint n){return atoms[n];};
        inline atom& operator[](cuint n) const {return atoms[n];};
        inline atomid get_id(cuint n) {
            if (n > sz) return atomid(); return atomid(atoms + n,n);};
        inline uint size() const {return sz;};

        ~atomvec(){ delete [] atoms;};
};

/*!
A class for representing any grouping of atoms that is not the whole set of
atoms, such as a molecule, a side-chain, etc.
@ingroup atoms
*/
class subgroup : public atomgroup {
    protected:
        sptr<atomvec> atoms;
        vector<atomid> ids;
    public:
        subgroup(sptr<atomvec> atoms) : atoms(atoms){};
        atomvec &vec(){return *atoms;};
        inline atom& operator[](cuint n){return *ids[n];};
        inline atom& operator[](cuint n) const{return *ids[n];};
        inline atom& get(cuint n){return *ids[n];};
        inline void add(atomid a){
			std::vector<atomid>::iterator it = std::find(ids.begin(), ids.end(), a);
			if(it != ids.end())
				throw std::invalid_argument("Cannot add atomid to subgroup: it already exists.");
			return ids.push_back(a);
		};
        inline atomid get_id(cuint n) {return ids[n];};
        inline uint size() const {return (uint) ids.size();};
};

#endif
