#include "grpl/pf.h"

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <vector>

using namespace grpl::pf;

template <typename ST>
void echo(std::ofstream &out, ST state, int id) {
  out << state.time << "," << state.config.x() << "," << state.config.y() << "," << state.config[2] << ","
      << state.kinematics[POSITION] << "," << state.kinematics[VELOCITY] << ","
      << state.kinematics[ACCELERATION] << "," << state.curvature << "," << id << ","
      << ""
      << ","
      << ""
      << ","
      << ""
      << "," << std::endl;
}

void echo_limits(std::ofstream &out, double time, profile::trapezoidal::limits_t lim) {
  auto vel = lim.col(1);
  auto acc = lim.col(2);

  out << time << ",,,,," << vel[0] << "," << acc[0] << ",,-1,,,," << std::endl;
  out << time << ",,,,," << vel[1] << "," << acc[1] << ",,-1,,,," << std::endl;
}

void echo_simulation(std::ofstream &out, double time, Eigen::Vector3d config) {
  out << time << "," << config.x() << "," << config.y() << "," << config[2] << ",,,,,3,,,," << std::endl;
}

template <typename ST>
void echo_wheel(std::ofstream &out, ST state, transmission::dc_motor &motor, int id) {
  out << state.time << "," << state.position.x() << "," << state.position.y() << ","
      << ""
      << ","
      << ""
      << "," << state.kinematics[VELOCITY] << "," << state.kinematics[ACCELERATION] << ","
      << ""
      // << "," << id << "," << state.angular_speed << "," << state.torque << ","
      // << motor.signal_to_voltage(state.control_signal) << "," << motor.torque_to_current(state.torque)
      << "," << id << "," << motor.partial_signal_at_speed(state.angular_speed) << "," << motor.partial_signal_at_torque(state.torque) << ","
      << state.control_signal << "," << motor.torque_to_current(state.torque)
      << std::endl;
}

TEST(CDT, basic) {
  using hermite_t     = path::hermite_quintic;
  using profile_t     = profile::trapezoidal;
  using state_t       = coupled::state;
  using wheel_state_t = coupled::wheel_state;

  std::vector<path::augmented_arc2d> curves;
  std::array<hermite_t::waypoint, 2> wps{hermite_t::waypoint{{0, 0}, {5, 0}, {0, 0}},
                                         hermite_t::waypoint{{4, 4}, {0, 5}, {0, 0}}};

  std::vector<hermite_t> hermites;
  path::hermite_factory::generate<hermite_t>(wps.begin(), wps.end(), std::back_inserter(hermites),
                                             hermites.max_size());

  path::arc_parameterizer param;
  param.configure(0.01, 0.01);
  param.parameterize(hermites.begin(), hermites.end(), std::back_inserter(curves), curves.max_size());

  profile_t profile;
  // profile.set_timeslice(0.00001);
  double G = 12.75;

  transmission::dc_motor dualCIM{12.0, 5330 * 2.0 * constants::PI / 60.0 / G, 2 * 2.7, 2 * 131.0,
                                 2 * 2.41 * G};
  // coupled_t               model{dualCIM, dualCIM, 0.0762, 0.5, 25.0, 10.0};
  coupled::chassis                     chassis{dualCIM, dualCIM, 0.0762, 0.5, 25.0};
  coupled::causal_trajectory_generator gen;

  state_t state;

  std::ofstream pathfile("cdt.csv");
  pathfile << "t,x,y,heading,distance,velocity,acceleration,curvature,path,angular,torque,voltage,current\n";

  coupled::configuration_state centre{0, 0, 0};

  std::pair<wheel_state_t, wheel_state_t> split;

  std::cout << "TOR_max: " << dualCIM.torque(0, 1.0);

  for (double t = 0; !state.finished && t < 5; t += 0.01) {
    // std::cout << "AA," << split.second.angular_speed << std::endl;
    state = gen.generate(chassis, curves.begin(), curves.end(), profile, state, t);
    // std::cout << "AA," << (state.kinematics[1] * state.curvature) << std::endl;
    // std::cout << "AA," << state.kinematics[VELOCITY] << std::endl;
    echo(pathfile, state, 0);

    echo_limits(pathfile, t, profile.get_limits());

    split = chassis.split(state);
    echo_wheel(pathfile, split.first, dualCIM, 1);
    echo_wheel(pathfile, split.second, dualCIM, 2);

    double w = (split.second.kinematics[VELOCITY] -
                split.first.kinematics[VELOCITY]);  // track radius = 0.5, track diam = 1
    double v = (split.second.kinematics[VELOCITY] + split.first.kinematics[VELOCITY]) / 2.0;

    // TODO: Checks

    centre[2] += w * 0.01;
    centre[0] += v * cos(centre[2]) * 0.01;
    centre[1] += v * sin(centre[2]) * 0.01;

    echo_simulation(pathfile, t, centre);
  }
}