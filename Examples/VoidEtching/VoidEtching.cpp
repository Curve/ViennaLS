#include <iostream>

#include <lsAdvect.hpp>
#include <lsBooleanOperation.hpp>
#include <lsDomain.hpp>
#include <lsExpand.hpp>
#include <lsMakeGeometry.hpp>
#include <lsPrune.hpp>
#include <lsToSurfaceMesh.hpp>
#include <lsToVoxelMesh.hpp>
#include <lsVTKWriter.hpp>

/**
  3D Example showing how to use the library for topography
  simulation. A substrate with big air inclusions is etched isotropically.
  Voids are detected automatically and only etched once they are exposed.
  \example VoidEtching.cpp
*/

// implement own velocity field
class velocityField : public lsVelocityField<double> {
public:
  double getScalarVelocity(const std::array<double, 3> & /*coordinate*/,
                           int /*material*/,
                           const std::array<double, 3> & /*normalVector*/) {
    // isotropic etch rate
    return -1;
  }

  std::array<double, 3>
  getVectorVelocity(const std::array<double, 3> & /*coordinate*/,
                    int /*material*/,
                    const std::array<double, 3> & /*normalVector*/) {
    return std::array<double, 3>({});
  }
};

int main() {

  constexpr int D = 3;
  omp_set_num_threads(4);

  double extent = 30;
  double gridDelta = 1;

  double bounds[2 * D] = {-extent, extent, -extent, extent, -extent, extent};
  lsDomain<double, D>::BoundaryType boundaryCons[D];
  boundaryCons[0] = lsDomain<double, D>::BoundaryType::REFLECTIVE_BOUNDARY;
  boundaryCons[1] = lsDomain<double, D>::BoundaryType::REFLECTIVE_BOUNDARY;
  boundaryCons[2] = lsDomain<double, D>::BoundaryType::INFINITE_BOUNDARY;

  lsDomain<double, D> substrate(bounds, boundaryCons, gridDelta);

  double origin[D] = {0., 0., 0.};
  double planeNormal[D] = {0., 0., 1.};

  lsMakeGeometry<double, D>(substrate, lsPlane<double, D>(origin, planeNormal))
      .apply();

  {
    // create spheres used for booling
    std::cout << "Creating spheres..." << std::endl;
    lsDomain<double, D> sphere(bounds, boundaryCons, gridDelta);
    origin[0] = -12;
    origin[1] = -5;
    origin[2] = -15;
    double radius = 10;
    lsMakeGeometry<double, D>(sphere, lsSphere<double, D>(origin, radius))
        .apply();
    lsBooleanOperation<double, D> boolOp(
        substrate, sphere, lsBooleanOperationEnum::RELATIVE_COMPLEMENT);
    boolOp.apply();

    origin[0] = -7;
    origin[1] = -30;
    origin[2] = -20;
    radius = 8;
    lsMakeGeometry<double, D>(sphere, lsSphere<double, D>(origin, radius))
        .apply();
    // reference to substrate and sphere are kept in boolOp
    boolOp.apply();

    origin[0] = 5;
    origin[1] = 15;
    origin[2] = -2;
    radius = 8;
    lsMakeGeometry<double, D>(sphere, lsSphere<double, D>(origin, radius))
        .apply();
    boolOp.apply();

    origin[0] = 2;
    origin[1] = 8;
    origin[2] = -27;
    radius = 8;
    lsMakeGeometry<double, D>(sphere, lsSphere<double, D>(origin, radius))
        .apply();
    boolOp.apply();
  }

  // Now etch the substrate isotropically
  velocityField velocities;

  std::cout << "Advecting" << std::endl;

  lsAdvect<double, D> advectionKernel;
  advectionKernel.insertNextLevelSet(substrate);
  advectionKernel.setVelocityField(velocities);
  advectionKernel.setIgnoreVoids(true);

  // Now advect the level set 50 times, outputting every
  // advection step. Save the physical time that
  // passed during the advection.
  double passedTime = 0.;
  unsigned numberOfSteps = 50;
  for (unsigned i = 0; i < numberOfSteps; ++i) {
    std::cout << "\rAdvection step " + std::to_string(i) + " / "
              << numberOfSteps << std::flush;
    lsMesh mesh;
    lsToSurfaceMesh<double, D>(substrate, mesh).apply();
    lsVTKWriter(mesh, "void-" + std::to_string(i) + ".vtk").apply();

    advectionKernel.apply();
    passedTime += advectionKernel.getAdvectionTime();
  }
  std::cout << std::endl;

  std::cout << "Time passed during advection: " << passedTime << std::endl;

  return 0;
}
