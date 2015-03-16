#include "box.hpp"

Vec LeesEdwardsBox::diff(Vec r1, Vec r2){
    flt Ly = boxsize[1];
    flt dy = r1[1]-r2[1];
    int im = (int) round(dy / Ly);
    dy = dy - (im*Ly);

    flt Lx = boxsize[0];
    flt dx = r1[0] - r2[0];
    dx = dx - round((dx/Lx)-im*gamma)*Lx-im*gamma*Lx;

    #ifdef VEC2D
    return Vec(dx, dy);
    #endif
    #ifdef VEC3D
    flt dz = remainder(r1[2] - r2[2], boxsize[2]);
    return Vec(dx, dy, dz);
    #endif
};

array<int,NDIM> LeesEdwardsBox::box_round(Vec r1, Vec r2){
    Vec dr = r1 - r2;
    array<int,NDIM> boxes;
    boxes[1] = (int) round(dr[1] / boxsize[1]);
    boxes[0] = (int) round((dr[0]/boxsize[0])-boxes[1]*gamma);
    #ifdef VEC3D
    boxes[2] = (int) round(dr[2] / boxsize[2]);
    #endif
    return boxes;
};

Vec LeesEdwardsBox::diff(Vec r1, Vec r2, array<int,NDIM> boxes){
    Vec dr = r1 - r2;
    dr[0] -= (boxes[0] + boxes[1]*gamma)*boxsize[0];
    dr[1] -= boxes[1]*boxsize[1];
    #ifdef VEC3D
    dr[2] -= boxes[2]*boxsize[2];
    #endif
    return dr;
}

SCBox::SCBox(flt L, flt R) : L(L), R(R){};

flt SCBox::V(){
    #ifdef VEC2D
    return R*L + R*R*M_PI;
    #else
    flt endcap = R*R*M_PI;
    return endcap*L + endcap*R*4/3;
    #endif
};

Vec SCBox::dist(Vec r1){
    // Vector to nearest point on central line
    if(r1[0] < -L/2) r1[0] += L/2;
    else if(r1[0] > L/2) r1[0] -= L/2;
    else r1[0] = 0;
    return r1;
};

Vec SCBox::edgedist(Vec r1){
    // Vector to nearest edge
    if(r1[0] < -L/2) r1[0] += L/2;
    else if(r1[0] > L/2) r1[0] -= L/2;
    else r1[0] = 0;

    flt dmag = r1.mag(); // distance to center
    if(dmag == 0){
        #ifdef VEC2D
        return Vec(0, R);
        #else
        return Vec(0, R, 0);
        #endif
    }
    return r1 * ((R-dmag)/dmag);
}

bool SCBox::inside(Vec r1, flt buffer){
    if(r1[0] < -L/2) r1[0] += L/2;
    else if(r1[0] > L/2) r1[0] -= L/2;
    else r1[0] = 0;
    flt newR = R - buffer;
    return r1.sq() < newR*newR;
};

Vec SCBox::randLoc(flt min_dist_to_wall){
    if(min_dist_to_wall >= R) return Vec();
    Vec v;
    flt Rmin = R - min_dist_to_wall;
    flt Rminsq = pow(Rmin, 2.0);
    while(true){
        v = randVecBoxed();
        v[0] -= 0.5;
        v[0] *= (L + 2*Rmin);

        v[1] -= 0.5;
        v[1] *= 2*Rmin;

        #ifndef VEC2D
        v[2] -= 0.5;
        v[2] *= 2*Rmin;
        #endif

        flt distsq = pow(v[1],2);
        if(abs(v[0]) >= L/2.0) distsq += pow(abs(v[0]) - L/2.0, 2.0);


        #ifndef VEC2D
        distsq += pow(v[2],2);
        #endif

        if(distsq <= Rminsq) break;
    };
    return v;
};

Vec AtomGroup::com() const{
    Vec v = Vec();
    for(unsigned int i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        flt curmass = (*this)[i].m;
        v += (*this)[i].x * curmass;
    }
    return v / mass();
};

flt AtomGroup::mass() const{
    flt m = 0;
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        m += (*this)[i].m;
    }
    return m;
};

Vec AtomGroup::comv() const {
    return momentum() / mass();
};

Vec AtomGroup::momentum() const{
    Vec tot = Vec();
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        flt curmass = a.m;
        tot += a.v * curmass;
    }
    return tot;
};

flt AtomGroup::gyradius() const{
    Vec avgr = Vec();
    for(uint i = 0; i<size(); i++){
        avgr += (*this)[i].x;
    }
    avgr /= size(); // now avgr is the average location, akin to c.o.m.
    flt Rgsq = 0;
    for(uint i = 0; i<size(); i++){
        Rgsq += ((*this)[i].x - avgr).sq();
    }

    return sqrt(Rgsq/size());
};

#ifdef VEC3D
Vec AtomGroup::angmomentum(const Vec &loc, Box &box) const{
    Vec tot = Vec();
    for(uint i=0; i<size(); i++){
        flt curmass = (*this)[i].m;
        if(curmass <= 0 or curmass) continue;
        Vec newloc = box.diff((*this)[i].x, loc);
        tot += newloc.cross((*this)[i].v) * curmass; // r x v m = r x p
    }
    return tot;
};
#elif defined VEC2D
flt AtomGroup::angmomentum(const Vec &loc, Box &box) const{
    flt tot = 0;
    Vec newloc;
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        newloc = box.diff(a.x, loc);
        tot += newloc.cross(a.v) * a.m; // r x v m = r x p
    }
    return tot;
};
#endif

#ifdef VEC3D
flt AtomGroup::moment(const Vec &loc, const Vec &axis, Box &box) const{
    if (axis.sq() == 0) return 0;
    flt tot = 0;
    Vec newloc;
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        newloc = box.diff(a.x, loc).perpto(axis);
        tot += newloc.dot(newloc) * a.m;
    }
    return tot;
};

Matrix<flt> AtomGroup::moment(const Vec &loc, Box &box) const{
    Matrix<flt> I;
    for(uint i=0; i<size(); i++){
        flt curmass = (*this)[i].m;
        if(curmass <= 0 or isinf(curmass)) continue;
        Vec r = box.diff((*this)[i].x, loc);
        flt x = r.getx(), y = r.gety(), z = r.getz();
        I[0][0] += curmass * (y*y + z*z);
        I[1][1] += curmass * (x*x + z*z);
        I[2][2] += curmass * (x*x + y*y);
        I[0][1] -= curmass * (x*y);
        I[0][2] -= curmass * (x*z);
        I[1][2] -= curmass * (y*z);
    }
    I[1][0] = I[0][1];
    I[2][0] = I[0][2];
    I[2][1] = I[1][2];
    return I;
};

Vec AtomGroup::omega(const Vec &loc, Box &box) const{
    Matrix<flt> Inv = moment(loc, box).SymmetricInverse();
    return Inv * (angmomentum(loc, box));
};

void AtomGroup::addOmega(Vec w, Vec loc, Box &box){
    for(uint i=0; i<size(); i++){
        Vec r = box.diff((*this)[i].x, loc);
        (*this)[i].v -= r.cross(w);
    }
};
#elif defined VEC2D
flt AtomGroup::moment(const Vec &loc, Box &box) const{
    flt tot = 0;
    Vec newloc;
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        newloc = box.diff(a.x, loc);
        tot += newloc.dot(newloc) * a.m;
    }
    return tot;
};

void AtomGroup::addOmega(flt w, Vec loc, Box &box){
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m <= 0 or isinf(a.m)) continue;
        Vec r = box.diff(a.x, loc);
        a.v += r.perp().norm()*w;
    }
};
#endif

flt AtomGroup::kinetic(const Vec &originvelocity) const{
    flt totE = 0;
    Vec curv;
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m == 0 or isinf(a.m)) continue;
        curv = a.v - originvelocity;
        totE += a.m/2 * curv.dot(curv);
    }
    return totE;
};

void AtomGroup::addv(Vec v){
    for(uint i=0; i<size(); i++){
        (*this)[i].v += v;
    }
};

void AtomGroup::randomize_velocities(flt T){
    for(uint i=0; i<size(); i++){
        atom& a = (*this)[i];
        if(a.m == 0 or isinf(a.m)) continue;
        a.v = randVec() * sqrt(T*NDIM/a.m);
    }
};

void AtomGroup::resetForces(){
    for(uint i=0; i<size(); i++){
        (*this)[i].f = Vec();
    }
};

//~ void AtomGroup::vverlet1(const flt dt){
    //~ for(uint i=0; i<size(); i++){
        //~ (*this)[i].x += (*this)[i].v * dt + (*this)[i].a * (dt*dt/2);
        //~ (*this)[i].v += (*this)[i].a * (dt/2);
    //~ }
//~ };

//~ void AtomGroup::vverlet2(const flt dt){
    //~ for(uint i=0; i<size(); i++){
        //~ (*this)[i].v += (*this)[i].a * (dt/2);
    //~ }
//~ };

void LeesEdwardsBox::shear(flt dgamma, AtomGroup &atoms){
    // TODO: is this right for non-square boxes?
    // Is gamma Δx / Lx or Δx / Ly?
    for(uint i=0; i<atoms.size(); i++){
        //flt dy = remainder(atoms[i].x[1], boxsize[1]);
        flt dy = atoms[i].x[1];
        atoms[i].x[0] += dy * dgamma;
    }

    gamma += dgamma;
};