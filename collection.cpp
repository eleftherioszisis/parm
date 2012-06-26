#include "collection.hpp"

collection::collection(vector<atomgroup*> gs, vector<interaction*> is,
              vector<statetracker*> ts)
        : groups(gs), interactions(is), trackers(ts), atoms(gs){
    update_trackers();
    setForces();
    update_trackers();
}

void collection::update_trackers(){
    vector<statetracker*>::iterator git;
    for(git = trackers.begin(); git<trackers.end(); git++){
        (*git)->update();
    }
}

flt collection::kinetic(){
    flt E=0;
    vector<atomgroup*>::iterator git;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &group = **git;
        E+= group.kinetic();
    }
    return E;
}

flt collection::energy(){
    flt E=0;
    vector<interaction*>::iterator it;
    for(it = interactions.begin(); it<interactions.end(); it++){
        interaction &inter = **it;
        E += inter.energy();
    }
    
    assert(not isnan(E));
    E += kinetic();
    assert(not isnan(E));
    //~ cout << "collection::energy(): " << E  << "\n";
    return E;
};

flt collection::temp(){
    flt totatoms = 0;
    flt totkinetic = 0;
    Vec v = comv();
    vector<atomgroup*>::iterator git;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &group = **git;
        totkinetic += group.kinetic(v);
        //~ cout << "group K: " << group.kinetic(v) << ", totatoms: " << group.size() << "\n";
        totatoms += group.size();
    }
    //~ cout << "K: " << totkinetic << ", totatoms: " << totatoms << "\n";
    return totkinetic * 2 / (3*totatoms-3);
}

//~ Vec collection::com(){
    //~ vector<atomgroup*>::iterator git;
    //~ flt mass = 0;
    //~ Vec totcom = Vec(0,0,0);
    //~ for(git = groups.begin(); git<groups.end(); git++){
        //~ atomgroup &g = **git;
        //~ for(uint i = 0; i<g.size(); i++){
            //~ flt curmass = g.getmass(i);
            //~ mass += curmass;
            //~ totcom += g[i].x * curmass;
        //~ }
    //~ }
    //~ return totcom / mass;
//~ }
//~ 
//~ Vec collection::comv(){
    //~ vector<atomgroup*>::iterator git;
    //~ flt mass = 0;
    //~ Vec totcom = Vec(0,0,0);
    //~ for(git = groups.begin(); git<groups.end(); git++){
        //~ atomgroup &g = **git;
        //~ for(uint i = 0; i<g.size(); i++){
            //~ flt curmass = g.getmass(i);
            //~ mass += curmass;
            //~ totcom += g[i].v * curmass;
        //~ }
    //~ }
    //~ return totcom / mass;
//~ }

flt collection::gyradius(){
    vector<atomgroup*>::iterator git;
    Vec avgr = Vec(0,0,0);
    flt N = 0;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &g = **git;
        for(uint i = 0; i<g.size(); i++){
            avgr += g[i].x;
            N++;
        }
    }
    avgr /= N; // now avgr is the average location, akin to c.o.m.
    flt Rgsq = 0;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &g = **git;
        for(uint i = 0; i<g.size(); i++) Rgsq += (g[i].x - avgr).sq();
    }
    
    return sqrt(Rgsq/N);
}

void collection::setForces(){
    vector<atomgroup*>::iterator git;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &group = **git;
        group.resetForces();
    }
    
    vector<interaction*>::iterator it;
    for(it = interactions.begin(); it<interactions.end(); it++){
        interaction &inter = **it;
        inter.setForces();
    }
}

collectionSol::collectionSol(const flt dt, const flt damp,const flt T, 
        vector<atomgroup*> groups,vector<interaction*> interactions,
        vector<statetracker*> trackers) :
        collection(groups, interactions, trackers), dt(dt), damping(damp), desT(T){
    setCs();
};

void collectionSol::setCs(){
    if(damping == 0){
        c0 = 1; c1 = 1; c2 = .5;
        sigmar = 0; sigmav = 0; corr = 1;
        gauss.set(sigmar, sigmav, corr);
        return;
    }
    // from Allen and Tildesley, 262
    flt dampdt = damping * dt;
    c0 = exp(-dampdt);
    c1 = (-expm1(-dampdt))/dampdt;
    c2 = (1-c1)/dampdt;
    
    // note that for sigmar, sigmav, and corr, we have taken the
    // k_B*T/m out and put it in the timestep place
    // note that these are unitless sigmas and correlations,
    // so sigmar has a dt*sqrt(k T/m) removed, and sigmav has a sqrt(k T/m)
    // removed
    if(dampdt > 1e-4)
        sigmar = sqrt((1/dampdt) * (
            2-(-4*expm1(-dampdt) + expm1(-2*dampdt))/dampdt
            ));
    else
        sigmar = sqrt(2*dampdt/3 - dampdt*dampdt/2 + 7*dampdt*dampdt*dampdt/30);
    //~ cout << "setCs: " << -4*expm1(-dampdt) << ',' << expm1(-2*dampdt)
         //~ << ", (...): " << (-4*expm1(-dampdt) + expm1(-2*dampdt))/dampdt
         //~ << ", sigmar:" << sigmar << '\n';
    sigmav = sqrt(-expm1(-2*dampdt));
    flt exdpdt = (-expm1(-dampdt));
    corr = exdpdt*exdpdt/dampdt/sigmar/sigmav;
    //~ cout << "vals: " << c0 << ',' << c1 << ',' << c2 << "; T=" <<desT
         //~ << ", sigmas: (" << sigmar << ',' << sigmav << ',' << corr << ')'
         //~ << ", dt=" << dt << ", damping=" << damping << "\n";
    gauss.set(sigmar, sigmav, corr);
}

void collectionSol::timestep(){
    vector<atomgroup*>::iterator git;
    // From Allen and Tildesley 263, Verlet-like, our equations are
    // r(t+dt) = r(t) + c1 dt v(t) + c2 dt^2 a(t) + drG
    // v(t+dt) = c0 v(t) + (c1 - c2) dt a(t) + c2 dt a(t+dt) + dvG
    // I split this:
    // r(t+dt) = r(t) + c1 dt v(t) + c2 dt^2 a(t) + drG
    // vp(t) = c0 v(t) + (c1 - c2) dt a(t) + dvG
    // (set forces)
    // v(t+dt) = vp(t) + c2 dt a(t+dt)
    
    //Step 1: set atom.x = r(t+dt) and atom.v = vp(t)
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &m = **git;
        for(uint i=0; i<m.size(); i++){
            flt v0 = sqrt(desT/m.getmass(i));
            flt r0 = dt * v0;
            VecPair vecpair = gauss.genVecs();
            // vecpair[0] is drG, and vecpair[1] is dvG
            m[i].x += m[i].v * (c1 * dt) + m[i].a * (c2*dt*dt) + vecpair[0]*r0;
            m[i].v = m[i].v*c0 + m[i].a * (dt*(c1-c2)) + vecpair[1]*v0;
            //~ if(i==0 and git == groups.begin()) cout 
                //~ << "drG: " << vecpair[0].mag() 
                //~ << ", dvG: " << vecpair[1].mag()
                //~ << "\n";
        }
    }
    // Now we set forces and accelerations
    setForces();
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &m = **git;
        for(uint i=0; i<m.size(); i++){
            m[i].a = m[i].f / m.getmass(i);
        }
    }
    
    // And finish m[i].v
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &m = **git;
        for(uint i=0; i<m.size(); i++){
            m[i].v += m[i].a * (dt*c2);
        }
    }
    
    update_trackers();
    
    return;
    /* Honeycutt and Thirumalai
    //~ cout << "damping " << damping << "\n";
    vector<atomgroup*>::iterator git;
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &m = **git;
        for(uint i=0; i<m.size(); i++){
            // step 1: get new x
            m[i].x += (m[i].v * dt) + (m[i].a * (.5 * dt*dt));
            
            //(useful values)
            flt mass = m.getmass(i);
            flt curdamp = damping / mass;
            flt curdamp2 = dt * curdamp / 2;
            
            // step 2: make v1 (intermediate v)
            m[i].v += m[i].v * (-curdamp2 + curdamp2*curdamp2)
                        + m[i].f * (dt / 2 / mass);
        }
    }
    
    // step 3: set forces
    setForces();
    
    // step 4: intermediate acceleration
    for(git = groups.begin(); git<groups.end(); git++){
        atomgroup &m = **git;
        for(uint i=0; i<m.size(); i++){
            Vec g = gauss.generate();
            //~ cout << "g " << g << "\n";
            m[i].a = (m[i].f + g) / m.getmass(i);
            
            // step 5: finish v and a
            m[i].v += m[i].a * (dt/2);
            m[i].a -= m[i].v * damping;
        }
    }*/
}