#ifndef TIMING_H
#define TIMING_H

#include <omp.h>
#include <cassert>
#include <atomic>

class timing_t {
  double _total_time;
  int _steps;
  double _start_time;
  unsigned int _entrants;

public:
  timing_t(): _total_time(0), _steps(0), _start_time(-1), _entrants(0) {}

  void start_timing_round() {
    if (_entrants == 0) {
      _start_time = omp_get_wtime();
    }
    _entrants++;
  }

  void end_timing_round(int _stepsTaken) {
    assert(_entrants > 0);
    _entrants--;

    if (_entrants == 0) {
      _total_time += omp_get_wtime() - _start_time;
      _start_time = -1;
    }
    _steps += _stepsTaken;
  }

  double time_per_step() {
    return total_time() / _steps;
  }

  double total_time() {
    double m_start_time = _start_time;
    double m_total_time = _total_time;

    if (m_start_time < 0) {
      return m_total_time;
    } else {
      return m_total_time + omp_get_wtime() - m_start_time;
    }
  }
};

#endif /* TIMING_H */
