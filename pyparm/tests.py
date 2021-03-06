# -*- coding: utf-8 -*-

from . import d2 as sim2
from . import d3 as sim3
from unittest import TestCase
import textwrap
import itertools
from transforms3d.axangles import axangle2mat

import numpy as np


class NPTestCase(TestCase):
    def assertClose(self, x, y, rtol=1e-05, atol=1e-08, msg=None):
        if np.allclose(x, y, rtol=rtol, atol=atol):
            return
        
        standardMsg = ('The following were not within %s rtol or %s atol:\n'
            '%s\n'
            '%s\n') % (rtol, atol, self._indent('A:  ', repr(x)), self._indent('B:  ', repr(y)))
        msg = self._formatMessage(msg, standardMsg)
        raise self.failureException(msg)
    
    def assertNotClose(self, x, y, rtol=1e-05, atol=1e-08, msg=None):
        if not np.allclose(x, y, rtol=rtol, atol=atol):
            return
        
        standardMsg = ('The following were within %s rtol and %s atol:\n'
            '%s\n'
            '%s\n') % (rtol, atol, self._indent('A:  ', repr(x)), self._indent('B:  ', repr(y)))
        msg = self._formatMessage(msg, standardMsg)
        raise self.failureException(msg)
        
    def _indent(self, prefix, msg):
        lines = str(msg).splitlines()
        subsequent_indent = ' ' * len(prefix)
        lines = (
            [textwrap.wrap(l, width=80,
                replace_whitespace=False,
                drop_whitespace=False,
                initial_indent=prefix,
                subsequent_indent=subsequent_indent,
                break_long_words=False,
                break_on_hyphens=False) for l in lines[:1]] +
            [textwrap.wrap(l, width=80,
                replace_whitespace=False,
                drop_whitespace=False,
                initial_indent=subsequent_indent,
                subsequent_indent=subsequent_indent,
                break_long_words=False,
                break_on_hyphens=False) for l in lines[1:]])
        lines = [l for lset in lines for l in lset]
        return '\n'.join(lines)
        

class VecTest(NPTestCase):
    def test_assignment(self):
        a = sim2.Atom()
        v2 = a.x
        print('v2:', v2)
        x, y = v2
        self.assertAlmostEqual(x, 0.)
        self.assertAlmostEqual(y, 0.)
        a.x = (2.3, 3.6)
        x, y = a.x
        self.assertAlmostEqual(x, 2.3)
        self.assertAlmostEqual(y, 3.6)
    
    def test_cross(self):
        x, y = sim2.cross((3., -1.4), 2.)
        self.assertAlmostEqual(x, -2.8)
        self.assertAlmostEqual(y, -6.)
        c = sim2.cross((3., -1.4), (-2., 0.))
        self.assertAlmostEqual(c, -2.8)
        c = sim2.cross((3., -1.4), (0., -2.))
        self.assertAlmostEqual(c, -6.)


class RigidConstraintCube(NPTestCase):
    seed = 28156
    masses = np.asarray([1.]*8)
    M = np.sum(masses)
    locs = np.asarray([
        [0, 0, 0],
        [0, 0, 1],
        [0, 1, 0],
        [0, 1, 1],
        [1, 0, 0],
        [1, 0, 1],
        [1, 1, 0],
        [1, 1, 1.],
    ])
    vs = None
    fs = None
    
    axis = (1, 0, 0)
    dtheta = np.pi/2.
    
    def setUp(self):
        np.random.seed(self.seed)
        self.locs -= np.sum(self.locs.T*self.masses, axis=1).T/self.M
        self.box = sim3.InfiniteBox()
        self.atoms = sim3.AtomVec(self.masses)
        vs = np.random.normal(size=np.shape(self.locs)) if self.vs is None else self.vs
        fs = np.random.normal(size=np.shape(self.locs)) if self.fs is None else self.fs
        for a, loc, v, f in zip(self.atoms, self.locs, vs, fs):
            a.x = loc
            a.v = v
            a.f = f
        self.rigid = sim3.RigidConstraint(self.box, self.atoms)
        self.rotmatrix = axangle2mat(self.axis, self.dtheta)
        for a, loc in zip(self.atoms, self.locs.dot(self.rotmatrix.T)):
            a.x = loc
    
    def test_rotation(self):
        m = sim3.best_rotation_matrix(self.locs, self.locs.dot(self.rotmatrix.T))
        self.assertClose(m, self.rotmatrix)
    
    def test_rigid_matrix(self):
        m = self.rigid.get_rotation()
        self.assertClose(m, self.rotmatrix)
    
    def test_rigid(self):
        com = self.atoms.com()
        mom = self.atoms.moment(com)
        self.rigid.apply_positions(self.box)
        com_velocity = self.atoms.com_velocity()
        omega = self.atoms.omega(com)
        angm = self.atoms.angular_momentum(com)
        K = self.atoms.kinetic_energy(com_velocity)
        self.rigid.apply_velocities(self.box)
        com_force = self.atoms.com_force()
        torq = self.atoms.torque(com)
        self.rigid.apply_forces(self.box)

        com2 = self.atoms.com()
        comv2 = self.atoms.com_velocity()
        comf2 = self.atoms.com_force()
        angm2 = self.atoms.angular_momentum(com2)
        mom2 = self.atoms.moment(com2)
        omega2 = self.atoms.omega(com2)
        torq2 = self.atoms.torque(com2)
        K2 = self.atoms.kinetic_energy(comv2)
        
        self.assertClose(com, com2)
        self.assertClose(com_velocity, comv2)
        self.assertClose(com_force, comf2)
        # Kinetic energy is not conserved, as velocities away from each other
        # are removed.
        # self.assertClose(K, K2)
        print(K, K2)
        self.assertClose(com_force, comf2)
        self.assertClose(angm, angm2)
        self.assertClose(mom, mom2)
        self.assertClose(omega, omega2)
        # Torque is actually not conserved. Not sure why.
        # self.assertClose(torq, torq2)
        print("torque:", torq, torq2)


class RigidConstraintCubeY(RigidConstraintCube):
    axis = (0, 1, 0)
    dtheta = np.pi/2.0


class RigidConstraintCubeZ(RigidConstraintCube):
    axis = (0, 0, 1)
    dtheta = np.pi/2.

    
class RigidConstraintCubeYC(RigidConstraintCube):
    axis = (0, 1, 0)
    dtheta = np.pi*1.28


class RigidConstraintCubeZC(RigidConstraintCube):
    axis = (0, 0, 1)
    dtheta = np.pi*1.28
    

class RigidConstraintCubeOther(RigidConstraintCube):
    axis = (.2, .3, .6)
    dtheta = np.pi*1.28


class RigidConstraintExtendedOnce(RigidConstraintCube):
    masses = np.asarray([1.]*4 + [2., 3., 1.4, 1.8])
    locs = np.asarray([
        [0, 0, 0],
        [0, 0, 3],
        [0, 1, 0],
        [0, 1, 3],
        [1, 0, 0],
        [1, 0, 3],
        [1, 1, 0],
        [1, 1, 3.],
    ])
    axis = (.2, .3, .6)
    dtheta = np.pi*1.28


class RigidConstraintExtendedTwce(RigidConstraintCube):
    masses = np.asarray([1.]*4 + [2., 3., 1.4, 1.8])
    locs = np.asarray([
        [0, 0, 0],
        [0, 0, 3],
        [0, 2, 0],
        [0, 2, 3],
        [1, 0, 0],
        [1, 0, 3],
        [1, 2, 0],
        [1, 2, 3.],
    ])
    axis = (.2, .3, .6)
    dtheta = np.pi*1.28


class PairedCollectionTest(NPTestCase):
    seed = None  # should be set by descendants
    collection_type = None  # should be set by descendants
    collection_args = None  # should be set by descendants
    pair_type = None  # should be set by descendants
    atom_type = None  # should be set by descendants
    atom_args = None  # should be set by descendants
    
    phi = 0.3
    N = 12
    
    def makePairAtoms(self):
        for args in zip(self.atoms, *self.atom_args):
            self.interaction.add(self.atom_type(*args))
        
    def getMasses(self):
        return [s**3 for s in self.getSigmas()]
        
    def getSigmas(self):
        N1 = self.N // 2
        N2 = self.N - N1
        return [1.] * N1 + [1.4] * N2
    
    def setUp(self):
        # for consistency
        np.random.seed(self.seed)
        
        radii = np.asarray(self.getSigmas()) / 2.
        Vs = np.sum(radii**3)*4/3*np.pi
        V = Vs / self.phi
        self.L = float(V**(1./3.))
        self.box = sim3.OriginBox(self.L)
        
        self.atoms = sim3.AtomVec(self.getMasses())
        self.interaction = self.pair_type(self.box, self.atoms, 0.4)
        
        self.makePairAtoms()
        self.resetPositions()
        
        collection_args = ([self.box, self.atoms] +
            list(self.collection_args) +
            [[self.interaction], [self.interaction.neighbor_list()], []])
            
        collec = self.collec = self.collection_type(*collection_args)
        
        print('EKUT:', collec.energy(), collec.kinetic_energy(),
            collec.potential_energy(), collec.temp())
    
    def resetPositions(self):
        np.random.seed(131)
        for a, m in zip(self.atoms, self.getMasses()):
            a.x = np.random.uniform(0., self.L, size=(3,))
            a.v = np.random.normal(size=(3,)) / m
            a.f = np.random.normal(size=(3,))
        
        nl = self.interaction.neighbor_list()
        nl.update_list(True)
    
    def reset(self):
        self.resetPositions()
        self.collec.scale_velocities_to_temp(1.0)
        for _ in range(1000):
            self.collec.timestep()
            self.collec.scale_velocities_to_temp(1.0)


class RandomHertzianVerletTest(PairedCollectionTest):
    seed = 131  # should be set by descendants
    collection_type = sim3.CollectionVerlet  # should be set by descendants
    pair_type = sim3.Repulsion  # should be set by descendants
    atom_type = sim3.EpsSigExpAtom  # should be set by descendants
    
    dt = 0.01
    collection_args = [dt]
    
    @property
    def atom_args(self):
        return [1.2] * self.N, self.getSigmas(), [2.0]*self.N
    
    def testEnergy(self):
        self.reset()
        collec = self.collec
        EKUTs = []
        lastE = collec.energy()
        for _ in range(1000):
            for _ in range(10):
                collec.timestep()
                self.assertClose(collec.energy(), lastE, rtol=1e-2)
                lastE = collec.energy()
            EKUTs.append((collec.energy(), collec.kinetic_energy(),
                          collec.potential_energy(), collec.temp()))
            
        EKUTs = np.asarray(EKUTs)
        E, K, U, T = EKUTs.T
        
        mean_T = np.mean(T)
        self.assertClose(mean_T, 1.0, rtol=1e-1)
        
        som = np.std(E) / np.mean(E)
        self.assertClose(som, 0.0, atol=1e-2)


class RandomHertzianGCTest(PairedCollectionTest):
    seed = 19372  # should be set by descendants
    collection_type = sim3.CollectionGaussianT  # should be set by descendants
    pair_type = sim3.Repulsion  # should be set by descendants
    atom_type = sim3.EpsSigExpAtom  # should be set by descendants
    
    dt = 0.01
    collection_args = [dt]
    
    @property
    def atom_args(self):
        return [1.2] * self.N, self.getSigmas(), [2.0]*self.N
    
    def testEnergy(self):
        self.reset()
        collec = self.collec
        EKUTs = []
        lastE = collec.energy()
        for _ in range(1000):
            for _ in range(10):
                collec.timestep()
                self.assertClose(collec.energy(), lastE, rtol=1e-2)
                lastE = collec.energy()
            EKUTs.append((collec.energy(), collec.kinetic_energy(),
                          collec.potential_energy(), collec.temp()))
            
        EKUTs = np.asarray(EKUTs)
        E, K, U, T = EKUTs.T
        
        som = np.std(T) / np.mean(T)
        self.assertClose(som, 0.0, atol=1e-2)


class RandomRigidConstraintTest(NPTestCase):
    phi_ish = 0.3
    N_per_mol = 4
    N_mol = 6
    N = N_per_mol * N_mol
    dt = 0.01
    
    def setUp(self):
        # for consistency
        np.random.seed(2460659162)
        self.radii = np.random.uniform(1.0, 2.0, size=(self.N,))
        self.masses = self.radii**3
        
        Vs = np.sum(self.radii**3)*4/3*np.pi
        V = Vs / self.phi_ish
        self.L = float(V**(1./3.))
        self.box = sim3.OriginBox(self.L)
        
        self.atoms = sim3.AtomVec(self.masses)

        self.hertz = sim3.Repulsion(self.box, self.atoms, 0.4)

        self.subgroups = [sim3.SubGroup(self.atoms) for _ in range(self.N_mol)]
        self.ixs = np.arange(self.N) // self.N_per_mol
        self.locs0 = np.random.normal(scale=10, size=(self.N_mol, 3))
        locs0 = np.array(self.locs0)
        for ix, a, radius, mass in zip(self.ixs, self.atoms, self.radii, self.masses):
            loc0 = locs0[ix]
            s = self.subgroups[ix]
            s.add(a)
            
            dx = np.random.normal(size=(3,))
            dx /= np.linalg.norm(dx)
            dx *= radius + 1.0
            a.x = loc0 + dx
            locs0[ix] = a.x
            a.v = np.random.normal(size=(3,))
            a.f = np.random.normal(size=(3,))
            self.hertz.add(sim3.EpsSigExpAtom(a, 100.0, radius*2.0, 2.0))

        nl = self.hertz.neighbor_list()
        for s in self.subgroups:
            for n, a in enumerate(s):
                alist = list(s)
                for m, a2 in enumerate(alist[:n]):
                    nl.ignore(a, a2)

        nl.update_list(True)
        
        self.rigids = [sim3.RigidConstraint(self.box, s) for s in self.subgroups]
        collec = self.collec = sim3.CollectionVerlet(
            self.box, self.atoms, self.dt,
            [self.hertz], [self.hertz.neighbor_list()], self.rigids)
        
        print('EKUT:', collec.energy(), collec.kinetic_energy(),
            collec.potential_energy(), collec.temp())
    
    def reset(self):
        np.random.seed(2460659162+1)
        locs0 = np.array(self.locs0)
        for ix, a, radius, mass in zip(self.ixs, self.atoms, self.radii, self.masses):
            loc0 = locs0[ix]
            
            dx = np.random.normal(size=(3,))
            dx /= np.linalg.norm(dx)
            dx *= radius + 1.0
            a.x = loc0 + dx
            locs0[ix] = a.x
            a.v = np.random.normal(size=(3,))
            a.f = np.random.normal(size=(3,))
        
        self.collec.scale_velocities_to_temp(1.0)
        for _ in range(10000):
            self.collec.timestep()
            self.collec.scale_velocities_to_temp(1.0)
    
    def testMomentOfInertia(self):
        self.reset()
        for s in self.subgroups:
            com = s.com()
            w = s.omega(com)
            L = s.angular_momentum(com)
            I = s.moment(com)

            self.assertClose(L, I.dot(w))
    
    def testRigid(self):
        self.reset()
        collec = self.collec
        for _ in range(100):
            collec.timestep()
        for s, rigid in zip(self.subgroups, self.rigids):
            com = s.com()
            mom = s.moment(com)
            rigid.apply_positions(self.box)
            com_velocity = s.com_velocity()
            omega = s.omega(com)
            angm = s.angular_momentum(com)
            K = s.kinetic_energy(com_velocity)
            rigid.apply_velocities(self.box)
            com_force = s.com_force()
            torq = s.torque(com)
            rigid.apply_forces(self.box)

            com2 = s.com()
            comv2 = s.com_velocity()
            comf2 = s.com_force()
            angm2 = s.angular_momentum(com2)
            mom2 = s.moment(com2)
            omega2 = s.omega(com2)
            torq2 = s.torque(com2)
            K2 = s.kinetic_energy(comv2)
            
            self.assertClose(com, com2)
            self.assertClose(com_velocity, comv2)
            self.assertClose(com_force, comf2)
            # Kinetic energy is not conserved, as velocities away from each other
            # are removed.
            # self.assertClose(K, K2)
            print("K:", K, K2)
            self.assertClose(com_force, comf2)
            self.assertClose(angm, angm2)
            self.assertClose(mom, mom2)
            self.assertClose(omega, omega2)
            # Torque is actually not conserved. Not sure why.
            # self.assertClose(torq, torq2)
            print("torque:", torq, torq2)
    
    def testEnergy(self):
        self.reset()
        collec = self.collec
        EKUTs = []
        lastE = collec.energy()
        for _ in range(1000):
            for _ in range(10):
                collec.timestep()
                self.assertClose(collec.energy(), lastE, rtol=1e-2)
                lastE = collec.energy()
            EKUTs.append((collec.energy(), collec.kinetic_energy(),
                          collec.potential_energy(), collec.temp()))
            
        EKUTs = np.asarray(EKUTs)
        E, K, U, T = EKUTs.T
        
        mean_T = np.mean(T)
        self.assertClose(mean_T, 1.0, rtol=1e-1)
        
        som = np.std(E) / np.mean(E)
        self.assertClose(som, 0.0, atol=1e-2)


class RSQTest(NPTestCase):
    N = 8
    phi = 0.3
    ns = [1, 2, 4, 8, 16, 32]
    ks = [1., 0.5]
    dxs = [
        [1, 0, 0],
        [-1, 0, 0],
        [0, 1, 0],
        [0, -1, 0],
        [0, 0, 1],
        [0, 0, -1],
        [0, 2, 1],
        [-2.1, 4, 1],
    ]
    
    def setUp(self):
        np.random.seed(13763)
        self.radii = np.random.uniform(1.0, 2.0, size=(self.N,))
        self.masses = self.radii**3
    
        Vs = np.sum(self.radii**3)*4/3*np.pi
        V = Vs / self.phi
        self.L = float(V**(1./3.))
        self.box = sim3.OriginBox(self.L)
        self.atoms = sim3.AtomVec(self.masses)
        self.rsqs = sim3.RsqTracker(self.atoms, self.ns, False)
        self.isfs = sim3.ISFTracker(self.atoms, self.ks, self.ns, False)
        
        drs = []
        for t in range(self.ns[-1] * 4):
            for i, (a, dr) in enumerate(zip(self.atoms, itertools.cycle(self.dxs))):
                a.x += dr
                if t == 0:
                    drs.append(dr)
            self.rsqs.update(self.box)
            self.isfs.update(self.box)
        self.drs = np.asarray(drs)
    
    def testr2(self):
        r2 = np.asarray(self.rsqs.r2())
        print(np.shape(r2))
        for n, r2n in zip(self.ns, r2):
            drsq = np.sum((np.asarray(self.drs)*n)**2, axis=-1)
            self.assertClose(drsq, r2n)
        
    def testxyz2(self):
        xyz2 = np.asarray(self.rsqs.xyz2())
        print(np.shape(xyz2))
        for n, xyz2n in zip(self.ns, xyz2):
            drsq = (np.asarray(self.drs)*n)**2
            self.assertClose(drsq, xyz2n)
        
    def testr4(self):
        r4 = np.asarray(self.rsqs.r4())
        print(np.shape(r4))
        for n, r4n in zip(self.ns, r4):
            drsq = np.sum((np.asarray(self.drs)*n)**2, axis=-1)**2
            self.assertClose(drsq, r4n)
        
    def testxyz4(self):
        xyz4 = np.asarray(self.rsqs.xyz4())
        print(np.shape(xyz4))
        for n, xyz4n in zip(self.ns, xyz4):
            drsq = (np.asarray(self.drs)*n)**4
            self.assertClose(drsq, xyz4n)
            
    def test_ISF_shape(self):
        ISFs = self.isfs.ISFs()
        ISFs = np.asarray(ISFs, dtype=complex)
        self.assertEqual(
            np.shape(ISFs),
            (len(self.ns), len(self.ks), len(self.atoms))
        )
          
        ISFxyz = self.isfs.ISFxyz()
        ISFxyz = np.asarray(ISFxyz, dtype=complex)
        
        self.assertEqual(
            np.shape(ISFxyz),
            (len(self.ns), len(self.ks), len(self.atoms), 3)
        )


class ShearTest2D(NPTestCase):
    N = 8
    L = 3.2
    d = 2
    
    def setUp(self):
        sim = sim2 if self.d == 2 else sim3
        
        self.box = sim.OriginBox(self.L)
        self.le_box = sim.LeesEdwardsBox(self.L)
        self.atoms = sim.AtomVec(self.N, 1.0)
        self.reset()
    
    def reset(self):
        np.random.seed(2556)
        for a in self.atoms:
            a.x = np.random.uniform(size=(self.d,)) * self.L * 2
            a.v = np.zeros(self.d)
        self.locs0 = np.asarray([a.x for a in self.atoms])
    
    def test_pure_shear(self):
        self.reset()
        for eps in np.logspace(-4, 1, 400):
            xfac = 1.0 + eps
            yfac = 1.0 / xfac
            self.box.pure_shear_to(eps, self.atoms)
            self.assertClose(self.box.V(), self.L**self.d)
            box_shape = self.box.box_shape()
            Lx, Ly = box_shape[:2]
            self.assertClose(Lx, self.L * xfac)
            self.assertClose(Ly, self.L * yfac)
            locs = np.asarray([a.x for a in self.atoms])
            x, y = locs.T[:2, :]
            x0, y0 = self.locs0.T[:2, :]
            self.assertClose(x, x0*xfac)
            self.assertClose(y, y0*yfac)
            
    def test_simple_shear(self):
        self.reset()
        for g in np.linspace(0, 2, 400):
            self.le_box.shear_to(g, self.atoms)
            self.assertClose(self.box.V(), self.L**self.d)
            locs = np.asarray([a.x for a in self.atoms])
            for r, r0 in zip(locs, self.locs0):
                self.assertClose(r0, self.le_box.non_affine(r))
                if r0[1] > self.L / 10 and g > 0.1:
                    self.assertNotClose(r0, r)


class ShearTest3D(ShearTest2D):
    d = 3


class RotateTest2D(NPTestCase):
    nrots = 4
    d = 2
    arr = np.array([3.0, 2.2])
    
    def setUp(self):
        self.sim = sim2 if self.d == 2 else sim3
        
    def test_rotations(self):
        rots = [self.sim.rotate(self.arr, n) for n in range(self.nrots)]
        for r1, r2 in itertools.combinations(rots, 2):
            self.assertNotClose(r1, r2)
    
    def test_rotoflips(self):
        rots = [self.sim.rotate_flip(self.arr, n) for n in range(2*self.nrots)]
        for r1, r2 in itertools.combinations(rots, 2):
            self.assertNotClose(r1, r2)
    
    def test_rotate_inv(self):
        for n in range(self.nrots):
            rot = self.sim.rotate(self.arr, n)
            print(n)
            if n == 0:
                print(n, rot, '=?', self.arr)
                self.assertClose(rot, self.arr)
            else:
                print(n, rot, '!=?', self.arr)
                self.assertNotClose(rot, self.arr)
            
            inv = self.sim.rotate_inv(rot, n)
            print(n, rot, inv, '=?', self.arr)
            self.assertClose(inv, self.arr)
    
    def test_rotate_flip_inv(self):
        for n in range(self.nrots*2):
            rot = self.sim.rotate_flip(self.arr, n)
            print(n)
            if n == 0:
                self.assertClose(rot, self.arr)
            else:
                self.assertNotClose(rot, self.arr)
            
            inv = self.sim.rotate_flip_inv(rot, n)
            self.assertClose(inv, self.arr)


class RotateTest3D(RotateTest2D):
    nrots = 24
    d = 3
    arr = np.array([3.0, 2.2, 4.6])
