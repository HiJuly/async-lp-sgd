#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <random>
#include <functional>
#include <cassert>

#include "mnist.hpp"
#include "gd.hpp"

int main() {
  std::mt19937 gen(5);
  std::normal_distribution<float> normal_dist(0, 1);

  dataset_t train = get_train_dataset();
  dataset_t test = get_test_dataset();
  assert(train.dim == test.dim);
  assert(train.num_labels == test.num_labels);

  const unsigned int d = train.dim;
  const unsigned int c = train.num_labels;

  const unsigned int n_train = train.n;
  float *X_train = train.image.data();
  unsigned int *ys_idx_train = train.labels_idx.data();
  float *ys_oh_train = train.labels_oh.data();

  const unsigned int n_test = test.n;
  float *X_test = test.image.data();
  unsigned int *ys_idx_test = test.labels_idx.data();
  float *ys_oh_test = test.labels_oh.data();

  const unsigned int niter = 1000;

  gd_losses_t losses = sgd(X_train, ys_idx_train, ys_oh_train, n_train,
                           X_test, ys_idx_test, ys_oh_test, n_test,
                           d, c, niter, 0.001, 1 / (c * d),
                           16, 1234);

  size_t n_losses = losses.times.size();

  assert(losses.times.size() == n_losses);
  assert(losses.train_losses.size() == n_losses);
  assert(losses.train_errors.size() == n_losses);
  assert(losses.test_losses.size() == n_losses);
  assert(losses.test_errors.size() == n_losses);

  for (unsigned int i = 0; i < n_losses; i++) {
    printf("%f %f %f %f %f\n",
           losses.times[i],
           losses.train_losses[i],
           losses.train_errors[i],
           losses.test_losses[i],
           losses.test_errors[i]
           );
  }
}
