#include <gtest/gtest.h>
#include <grpl/path/hermite.h>
#include <grpl/units.h>

#include <fstream>
#include <iostream>

using namespace grpl;
using namespace grpl::units;
using namespace grpl::path;

TEST(Path, Hermite) {
  using hermite_t = hermite<Distance, 2>;
  hermite_t::waypoint wp0 = { hermite_t::vector_t{ 2*m, 2*m }, hermite_t::vector_t{ 5*m, 0*m } },
                      wp1 = { hermite_t::vector_t{ 4*m, 5*m }, hermite_t::vector_t{ 0  , 5*m } };

  hermite_t hermite(wp0, wp1, 100000);
  std::ofstream outfile("hermite.csv");
  outfile << "t,x,y\n";
  std::cout << hermite.calculate_arc_length().as(m) << std::endl;

  for (double t = 0; t <= 1; t += 0.001) {
    auto pt = hermite.calculate(t);
    outfile << t << "," << pt[0].as(m) << "," << pt[1].as(m) << std::endl;
  }
}