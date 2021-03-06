#include <stdlib.h>
#include <float.h>
#include <atomic>
#include <random>
#include <functional>
#include <vector>
#include "string.h"

#include "gd.hpp"
#include "loss.hpp"
#include "mblas.hpp"
#include "timing.hpp"

void sgd
(
 const float* __restrict__ X_train_in,     // n x d
 const int* __restrict__ ys_idx_train_in,  // n x 1
 const float* __restrict__ ys_oh_train_in, // n x 1
 const size_t n_train,                     // num training samples
 const float* __restrict__ X_test_in,      // n x d
 const int* __restrict__ ys_idx_test_in,   // n x 1
 const float* __restrict__ ys_oh_test_in,  // n x 1
 const size_t n_test,                      // num training samples
 const size_t d,                           // data dimensionality
 const size_t c,                           // num classes
 const unsigned int niter,      // number of iterations to run
 const float alpha,             // step size
 const float lambda,            // regularization parameter
 const float beta_1,            // ema parameter of 1st moment
 const float beta_2,            // ema parameter of 2nd moment
 const size_t batch_size,       // batch size
 const unsigned int seed        // random seed
 ) {
#if defined(ADAM_SERIAL) || defined(SVRG) || defined(SGD)
  omp_set_num_threads(1);
#endif

  // initialize randoms
  std::mt19937 gen_main(seed);
  std::vector<std::mt19937> gen_all;
  for (int t = 0; t < omp_get_max_threads(); t++) {
    gen_all.push_back(std::mt19937(seed + t));
  }
  std::normal_distribution<float> normal_dist(0, 1);
  std::uniform_int_distribution<unsigned int> uniform_dist(0, n_train-1);
  std::uniform_int_distribution<int> thread_dist(0, omp_get_max_threads() - 1);

  const size_t X_lda = ALIGN_ABOVE(d);
  assert(X_lda % ALIGNMENT == 0);
  __assume(X_lda % ALIGNMENT == 0);
  const size_t ys_oh_lda = ALIGN_ABOVE(c);
  assert(ys_oh_lda % ALIGNMENT == 0);
  __assume(ys_oh_lda % ALIGNMENT == 0);

  float* __restrict__ W = (float*) ALIGNED_MALLOC(d * sizeof(float));
  __assume_aligned(W, ALIGNMENT);

  float* __restrict__ W_tilde = (float*) ALIGNED_MALLOC(d * sizeof(float));
  __assume_aligned(W_tilde, ALIGNMENT);
  float* __restrict__ mu_tilde = (float*) ALIGNED_MALLOC(d * sizeof(float));
  __assume_aligned(mu_tilde, ALIGNMENT);

  const float* __restrict__ X_train = (float*) ALIGNED_MALLOC(n_train * X_lda * sizeof(float));
  __assume_aligned(X_train, ALIGNMENT);
  for (int i = 0; i < n_train; i++) {
    for (int j = 0; j < d; j++) {
      ((float*)X_train)[i * X_lda + j] = X_train_in[i * d + j];
    }
  }

  const int* __restrict__ ys_idx_train = (int*) ALIGNED_MALLOC(n_train * sizeof(int));
  __assume_aligned(ys_idx_train, ALIGNMENT);
  memcpy((int*) ys_idx_train, ys_idx_train_in, n_train * sizeof(int));
  const float* __restrict__ ys_oh_train = (float*) ALIGNED_MALLOC(n_train * ys_oh_lda * sizeof(float));
  __assume_aligned(ys_oh_train, ALIGNMENT);
  for (int i = 0; i < n_train; i++) {
    memcpy((float*) &ys_oh_train[i * ys_oh_lda], &ys_oh_train_in[i * c], c * sizeof(float));
  }

  const float* __restrict__ X_test = (float*) ALIGNED_MALLOC(n_test * X_lda * sizeof(float));
  for (int i = 0; i < n_test; i++) {
    memcpy((float*) &X_test[i * X_lda], &X_test_in[i * d], d * sizeof(float));
  }
  const int* __restrict__ ys_idx_test = (int*) ALIGNED_MALLOC(n_test * sizeof(int));
  __assume_aligned(ys_idx_test, ALIGNMENT);
  memcpy((int*) ys_idx_test, ys_idx_test_in, n_test * sizeof(int));
  const float* __restrict__ ys_oh_test = (float*) ALIGNED_MALLOC(n_test * ys_oh_lda * sizeof(float));
  __assume_aligned(ys_oh_test, ALIGNMENT);
  for (int i = 0; i < n_test; i++) {
    memcpy((float*) &ys_oh_test[i * ys_oh_lda], &ys_oh_test_in[i * c], c * sizeof(float));
  }


  // gradient
  float* __restrict__ G_all = (float*) ALIGNED_MALLOC(ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  __assume_aligned(G_all, ALIGNMENT);
  memset(G_all, 0, ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  for (int t = 0; t < omp_get_max_threads(); t++) {
    for (int j = 0; j < d; j++) {
      G_all[t * ALIGN_ABOVE(d) + j] = 0;
    }
  }
  float* __restrict__ G_tilde_all = (float*) ALIGNED_MALLOC(ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  __assume_aligned(G_tilde_all, ALIGNMENT);

  // timing
  unsigned int* __restrict__ t_all = (unsigned int*) ALIGNED_MALLOC(omp_get_max_threads() * sizeof(unsigned int));
  __assume_aligned(t_all, ALIGNMENT);
  for (int t = 0; t < omp_get_max_threads(); t++) {
    t_all[t] = 1;
  }


#if defined(ADAM_SERIAL) || defined(ADAM_SHARED)
  float* __restrict__ m = (float*) ALIGNED_MALLOC(d * sizeof(float));
  memset(m, 0, d * sizeof(float));
  __assume_aligned(m, ALIGNMENT);
  float* __restrict__ v = (float*) ALIGNED_MALLOC(d * sizeof(float));
  __assume_aligned(v, ALIGNMENT);
  memset(v, 0, d * sizeof(float));
#elif defined(ADAM_PRIVATE)
  float* __restrict__ m_all = (float*) ALIGNED_MALLOC(ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  __assume_aligned(m_all, ALIGNMENT);
  memset(m_all, 0, ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  float* __restrict__ v_all = (float*) ALIGNED_MALLOC(ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
  __assume_aligned(v_all, ALIGNMENT);
  memset(v_all, 0, ALIGN_ABOVE(d) * omp_get_max_threads() * sizeof(float));
#endif

  // tmp array for holding batch X
  float *batch_X = (float*) ALIGNED_MALLOC(sizeof(float) * batch_size * X_lda);
  __assume_aligned(batch_X, ALIGNMENT);
  // tmp array for holding one-hot batch ys
  float *batch_ys_oh = (float*) ALIGNED_MALLOC(sizeof(float) * batch_size * ys_oh_lda);
  __assume_aligned(batch_ys_oh, ALIGNMENT);
  // tmp array for holding idx batch ys
  int *batch_ys_idx = (int*) ALIGNED_MALLOC(sizeof(int) * batch_size);
  __assume_aligned(batch_ys_idx, ALIGNMENT);
  // vector used for fisher-yates-esque batch selection w/out replacement
  unsigned int *batch_idx_all = (unsigned int*) ALIGNED_MALLOC(sizeof(unsigned int) * ALIGN_ABOVE(n_train) * omp_get_max_threads());
  __assume_aligned(batch_idx_all, ALIGNMENT);

  // collection of uniform distributions for batch selection
  std::vector< std::uniform_int_distribution<unsigned int> > batch_dists;
  // scratch space
  const size_t scratch_size_per_thread = scratch_size(n_train + n_test,d,c);
  assert(scratch_size_per_thread % ALIGNMENT == 0);
  float* __restrict__ scratch_all = (float*) ALIGNED_MALLOC(scratch_size_per_thread * omp_get_max_threads() * sizeof(float));
  __assume_aligned(scratch_all, ALIGNMENT);

  // initialize the batch selection vector (invariant is that it's an unordered set)
  for (int t = 0; t < omp_get_max_threads(); t++) {
    for (unsigned int i = 0; i < n_train; i++) {
      batch_idx_all[t * ALIGN_ABOVE(n_train) + i] = i;
    }
  }

  // initialize each distribution (this is ugly... TODO: any better way?)
  for (unsigned int i = 0; i < batch_size; i++) {
    batch_dists.push_back(std::uniform_int_distribution<unsigned int>(0, n_train - i - 1));
  }

  // initialize weight vector
#pragma vector aligned
  for (unsigned int j = 0; j < d; j++) {
    W[j] = normal_dist(gen_main);
  }

  loss_t train_loss, test_loss;

  train_loss = logistic_loss(W, X_train, X_lda, ys_idx_train, n_train,
                             d, lambda, scratch_all);

  test_loss = logistic_loss(W, X_test, X_lda, ys_idx_test, n_test,
                            d, lambda, scratch_all);

#ifdef RAW_OUTPUT
  printf(
         "%f"
#ifdef LOSSES
         " %f %f %f"
#endif /* LOSSES */
         "\n",
         0.0
#ifdef LOSSES
         , train_loss.loss,
         train_loss.error,
         test_loss.error
#endif /* LOSSES */
         );
#endif  /* RAW_OUTPUT */

#ifdef PROGRESS
  fprintf(stderr, "ITERATION"
#ifdef LOSSES
          " | TRAIN LOSS | TRAIN ERR | TEST ERR"
#endif /* LOSSES */
          "\n");
  fflush(stderr);
#endif /* PROGRESS */

#if defined(SVRG)
  unsigned int nepoch = 20;
  unsigned int niter_per_epoch = niter / nepoch;
#else
  unsigned int nepoch = 1;
  unsigned int niter_per_epoch = niter;
#endif

  timing_t full_timer = timing_t();
  full_timer.start_timing_round();

  for (unsigned int _epoch = 0; _epoch < nepoch; _epoch++) {
#if defined(SVRG)
    memcpy(W_tilde, W, d * sizeof(float));
    memset(mu_tilde, 0, d * sizeof(float));
    logistic_gradient_batch(mu_tilde, W_tilde, X_train, X_lda,
                            ys_idx_train,
                            n_train, d, lambda, scratch_all);
#endif

#pragma omp parallel for schedule(guided)
    for (unsigned int _iter = 0; _iter < niter_per_epoch; _iter++) {
      unsigned int tno = omp_get_thread_num();

      const int m_t = t_all[tno]++;

#if defined(ADAM_SERIAL) || defined(ADAM_PRIVATE) || defined(ADAM_SHARED)
      float t_exp;

#  if defined(ADAM_SERIAL) || defined(ADAM_SHARED)
      t_exp = m_t * omp_get_max_threads() + thread_dist(gen_all[tno]);

      float* __restrict__ m_m = m;
      float* __restrict__ m_v = v;
#  else
      t_exp = m_t;
      float* __restrict__ m_m = &m_all[ALIGN_ABOVE(d) * tno];
      float* __restrict__ m_v = &v_all[ALIGN_ABOVE(d) * tno];
#  endif /* ADAM_SHARED */

      __assume_aligned(m_m, ALIGNMENT);
      __assume_aligned(m_v, ALIGNMENT);

      float beta_1_t = powf(beta_1, t_exp);
      float beta_2_t = powf(beta_2, t_exp);

      float alpha_t = alpha * sqrtf(1 - beta_2_t) / (1 - beta_1_t) /sqrtf(m_t);
#endif

      float* __restrict__ scratch = &scratch_all[scratch_size_per_thread * tno];
      __assume_aligned(scratch, ALIGNMENT);

      float* __restrict__ G = &G_all[ALIGN_ABOVE(d) * tno];
      __assume_aligned(G, ALIGNMENT);

#if defined(SVRG)
      float* __restrict__ G_tilde = &G_tilde_all[ALIGN_ABOVE(d) * tno];
      __assume_aligned(G_tilde, ALIGNMENT);
#endif

      unsigned int* __restrict__ batch_idx = &batch_idx_all[ALIGN_ABOVE(n_train) * tno];
      __assume_aligned(batch_idx, ALIGNMENT);

      for (unsigned int bidx = 0; bidx < batch_size; bidx++) {
        const unsigned int rand_idx = batch_dists[bidx](gen_all[tno]);
        const unsigned int idx = batch_idx[rand_idx];
        batch_idx[rand_idx] = batch_idx[n_train - 1 - bidx];
        batch_idx[n_train - 1 - bidx] = idx;

        float *x_dst = &batch_X[bidx * X_lda];
        const float *x_src = &X_train[idx * X_lda];

#pragma vector aligned
        for (unsigned int j = 0; j < d; j++) {
          x_dst[j] = x_src[j];
        }

        batch_ys_idx[bidx] = ys_idx_train[idx];
        float *ys_dst = &batch_ys_oh[bidx * ys_oh_lda];
        const float *ys_src = &ys_oh_train[idx * ys_oh_lda];

#pragma vector aligned
        for (unsigned int k = 0; k < c; k++) {
          ys_dst[k] = ys_src[k];
        }
      }

      memset(G, 0, d * sizeof(float));
      logistic_gradient_batch(G, W, batch_X, X_lda,
                              batch_ys_idx,
                              batch_size, d, lambda, scratch);
#if defined(SVRG)
      memset(G_tilde, 0, d * sizeof(float));
      logistic_gradient_batch(G_tilde, W_tilde, batch_X, X_lda,
                              batch_ys_idx,
                              batch_size, d, lambda, scratch);
#endif

#pragma vector aligned
      for (unsigned int j = 0; j < d; j++) {
#if defined(ADAM_SERIAL) || defined(ADAM_PRIVATE) || defined(ADAM_SHARED)
        if (fabs(G[j]) > 1e-6) {
          m_m[j] = beta_1 * m_m[j] + (1 - beta_1) * G[j];
          m_v[j] = beta_2 * m_v[j] + (1 - beta_2) * G[j] * G[j];

          W[j] -= alpha_t * m_m[j] / (sqrtf(m_v[j]) + 1e-3);
        }
#elif defined(SGD)
        W[j] -= alpha * G[j] / sqrtf(m_t);
#elif defined(SVRG)
        W[j] -= alpha * (G[j] - G_tilde[j] + mu_tilde[j]);
#else
#  error Need a GD type
#endif
      }


#ifdef LOSSES
      train_loss = logistic_loss(W, X_train, X_lda, ys_idx_train, n_train,
                                 d, lambda, scratch);
      test_loss = logistic_loss(W, X_test, X_lda, ys_idx_test, n_test,
                                d, lambda, scratch);
#endif /* LOSSES */

#ifdef RAW_OUTPUT
      printf(
             "%f"
#ifdef LOSSES
             " %f %f %f"
#endif /* LOSSES */
             "\n",

             full_timer.total_time()
#ifdef LOSSES
             , train_loss.loss,
             train_loss.error,
             test_loss.error
#endif /* LOSSES */
             );
#endif  /* RAW_OUTPUT */

#ifdef PROGRESS
      unsigned int it = m_t * omp_get_max_threads();
      fprintf(stderr,
              "%9d"
#ifdef LOSSES
              " | %10.2f | %9.3f | %8.3f"
#endif /* LOSSES */
              "\r",
              it
#ifdef LOSSES
              ,train_loss.loss, train_loss.error, test_loss.error
#endif /* LOSSES */
              );
#endif /* PROGRESS */
    }
  }

#ifdef PROGRESS
  fprintf(stderr, "\n");
#endif /* PROGRESS */

  fprintf(stderr, "Final training loss: %f\n",  train_loss.loss);
  fprintf(stderr, "Final training error: %f\n", train_loss.error);
  fprintf(stderr, "Final testing error: %f\n",  test_loss.error);

  ALIGNED_FREE(W);
  ALIGNED_FREE((float*) X_train);
  ALIGNED_FREE((int*) ys_idx_train);
  ALIGNED_FREE((float*) ys_oh_train);
  ALIGNED_FREE((float*) X_test);
  ALIGNED_FREE((int*) ys_idx_test);
  ALIGNED_FREE((float*) ys_oh_test);
  ALIGNED_FREE((float*) scratch_all);
  ALIGNED_FREE(G_all);
  ALIGNED_FREE(batch_idx_all);
  ALIGNED_FREE(batch_X);
  ALIGNED_FREE(batch_ys_oh);
  ALIGNED_FREE(batch_ys_idx);
  ALIGNED_FREE(t_all);

#if defined(ADAM_SERIAL) || defined(ADAM_SHARED)
  ALIGNED_FREE(m);
  ALIGNED_FREE(v);
#elif defined(ADAM_PRIVATE)
  ALIGNED_FREE(m_all);
  ALIGNED_FREE(v_all);
#endif
}
