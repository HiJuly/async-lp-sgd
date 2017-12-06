#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <random>
#include <functional>
#include <cassert>

#include "data.hpp"
#include "gd.hpp"

int main() {
  init_data();

  dataset_t train = get_train_dataset();
  dataset_t test = get_test_dataset();
  assert(train.dim == test.dim);
  assert(train.num_labels == test.num_labels);

  const unsigned int d = train.dim;
  const unsigned int c = train.num_labels;

  const unsigned int n_train = train.n;
  float *X_train = train.image;
  int *ys_idx_train = train.labels_idx;
  float *ys_oh_train = train.labels_oh;

  const unsigned int n_test = test.n;
  float *X_test = test.image;
  int *ys_idx_test = test.labels_idx;
  float *ys_oh_test = test.labels_oh;

  const unsigned int niter = 1000000;

  gd_losses_t losses = sgd(X_train, ys_idx_train, ys_oh_train, n_train,
                           X_test, ys_idx_test, ys_oh_test, train.w_opt,
                           n_test, d, c, niter, .0001, 1, 0.9, 0.999,
                           32, 1234);

  size_t n_losses = losses.times.size();

  assert(losses.times.size() == n_losses);
  assert(losses.grad_sizes.size() == n_losses);

#ifdef LOSSES
  assert(losses.train_losses.size() == n_losses);
  assert(losses.train_errors.size() == n_losses);
  assert(losses.test_errors.size() == n_losses);
#endif /* LOSSES */

#ifndef RAW_OUTPUT
  for (unsigned int i = 0; i < n_losses; i++) {
    printf(
#ifdef LOSSES
           "%f %f %f %f %f\n",
#else
           "%f %f\n",
#endif /* LOSSES */
           losses.times[i],
           losses.grad_sizes[i]
#ifdef LOSSES
           , losses.train_losses[i],
           losses.train_errors[i],
           losses.test_errors[i]
#endif /* LOSSES */
           );
  }
#endif  /* RAW_OUTPUT */
}
