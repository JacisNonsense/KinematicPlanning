#pragma once

#include "grpl/profile/profile.h"
#include "grpl/path/path.h"
#include "grpl/util/vec.h"
#include "grpl/util/constants.h"

#include <Eigen/Dense>

#include <cmath>
#include <iostream>

namespace grpl {
namespace system {

  template <typename path_t, typename profile_t>
  class coupled_drivetrain {
  public:
    static_assert(path_t::DIMENSIONS == 2, "Path must function in exactly 2 Dimensions for Coupled Drivetrain!");
    using kinematics_t = Eigen::Matrix<double, path_t::DIMENSIONS, profile_t::ORDER>;
    using kinematics_1d_t = typename profile_t::kinematics_1d_t;
    using vector_t = typename path_t::vector_t;

    void apply_limit(int derivative_idx, double maximum) { _limits[derivative_idx] = maximum; }
    kinematics_1d_t &get_limits() { return _limits; }
    void set_limits(kinematics_1d_t &other) { _limits = other; }

    void set_trackwidth(double tw) { _trackwidth = tw; }
    double get_trackwidth() const { return _trackwidth; }

    struct coupled_side {
      double t;
      double d;
      kinematics_t k;
    };

    struct state {
      coupled_side l, c, r;
      kinematics_1d_t a;
      bool done;
    };

    state generate(path_t *path, profile_t *profile, state &last, double time) {
      state output;
      double path_len = path->get_arc_length();

      double cur_distance = last.c.d;
      if (cur_distance >= path_len) {
        output = last;
        output.done = true;
        return output;
      }
      
      double path_progress = (cur_distance / path_len);

      vector_t center = path->calculate(path_progress);
      vector_t center_prime = path->calculate_slope(path_progress);

      double dt = time - last.c.t;
      bool first = (dt < 0.00000001); // to avoid dt = 0 errors

      double angle = atan2(center_prime[1], center_prime[0]);
      vector_t  unit_angle = vec_polar(1, angle),
                tw_angle = vec_polar(_trackwidth / 2.0, angle);

      output.a[0] = angle;  // angle difference needs to be bound, but the derivatives do not
      for (size_t i = 1; i < profile_t::ORDER; i++) {
        double diff = output.a[i-1] - last.a[i-1];
        output.a[i] = first ? 0 : diff / dt;
      }

      // Half of difference between left and right side for each of { vel, acc, etc }
      kinematics_1d_t differentials = output.a * _trackwidth / 2.0;
      kinematics_1d_t new_limits = _limits - differentials.cwiseAbs();

      typename profile_t::segment_t segment;
      segment.time = last.c.t;
      for (size_t i = 0; i < profile_t::ORDER; i++) {
        segment.k[i] = last.c.k.col(i).dot(unit_angle);
      }
      segment.k[0] = cur_distance;

      profile->set_goal(path_len);
      profile->set_limits(new_limits);
      segment = profile->calculate(segment, time);

      output.c.t = output.l.t = output.r.t = time;

      output.c.k = unit_angle * segment.k;
      output.c.d = segment.k[0];
      
      output.l.k = output.c.k - tw_angle * output.a;
      output.r.k = output.c.k + tw_angle * output.a;

      output.c.k.col(0) = center;
      output.l.k.col(0) = output.c.k.col(0) - vec_polar(_trackwidth / 2.0, output.a[0] - PI/2.0);
      output.r.k.col(0) = output.c.k.col(0) + vec_polar(_trackwidth / 2.0, output.a[0] - PI/2.0);

      output.l.d = last.l.d + output.l.k.col(1).dot(unit_angle)*dt; // TODO: These have to be projected, not just find length
      output.r.d = last.r.d + output.r.k.col(1).dot(unit_angle)*dt;

      output.done = false;
      return output;
    }

  protected:
    double _trackwidth;
    kinematics_1d_t _limits;
  };

}
}