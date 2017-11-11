#ifdef OSX_ACCELERATE
#  include <Accelerate/Accelerate.h>
#elif defined(__GNUC__) || defined(__GNUG__)
#  include <cblas.h>
#else
#  error you gotta have some blas cmon
#endif

#ifdef OSX_ACCELERATE
#  define SAXPBY catlas_saxpby
#else
#  define SAXPBY cblas_saxpby
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <random>

#include "mnist.h"

int main() {
  printf("Hello, world!\n");
}

void sgd_impl
(float* __restrict__ w,
 float* __restrict__ grad,
 const float* __restrict__ xs,
 const char* __restrict__ ys,
 const size_t n,
 const size_t d,
 const unsigned int niter,
 const float alpha,
 const float beta,
 const float lambda,
 std::function<int()> selector
 ) {
  for (unsigned int iter = 0; iter < niter; iter++) {
    const int idx = selector();
    const float *x = &xs[idx * d];
    const char y = ys[idx];

    float wTx = cblas_sdot(d, x, 1, w, 1);

    const float scale = -y / (1 + expf(y * wTx));

    SAXPBY(d, -alpha * scale, x, 1, -2 * lambda * alpha, w, 1);
  }
}

float loss
(const float* __restrict__ w,
 const float* __restrict__ xs,
 const char* __restrict__ ys,
 const size_t n,
 const size_t d,
 const float lambda
 ) {
  float loss = 0;

  for (unsigned int idx = 0; idx < n; idx++) {
    const float *x = &xs[idx * d];
    const char y = ys[idx];

    float wTx = cblas_sdot(d, x, 1, w, 1);
    float wTw = cblas_sdot(d, w, 1, w, 1);

    loss += log2f(1 + expf(-y * wTx)) + lambda * wTw;
  }

  return loss;
}

void sgd
(
 float* __restrict__ w,
 const float* __restrict__ xs,
 const char* __restrict__ ys,
 const size_t n,
 const size_t d,
 const unsigned int niter,
 const float alpha,
 const float beta,
 const float lambda,
 const unsigned int seed,
 const unsigned int report_loss_every,
 float * __restrict__ losses
 ) {
  std::mt19937 gen(seed);
  std::normal_distribution<float> normal_dist(0, 1);
  std::uniform_int_distribution<int> uniform_dist(1,n);
  auto selector = std::bind ( uniform_dist, gen );

  float* __restrict__ grad = (float*) calloc(d, sizeof(float));

  for (int j = 0; j < d; j++) {
    w[j] = normal_dist(gen);
  }

  if (report_loss_every == 0) {
    sgd_impl(w, grad, xs, ys, n, d, niter, alpha, beta, lambda, selector);
  } else {
    for (int iter = 0; iter < niter; iter += report_loss_every) {
      int iter_count = std::min(niter - iter, report_loss_every);
      sgd_impl(w, grad, xs, ys, n, d, iter_count, alpha, beta, lambda, selector);
      losses[iter / report_loss_every] = loss(w, xs, ys, n, d, lambda);
    }
  }

  free(grad);
}
