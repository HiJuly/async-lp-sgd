#include <stdio.h>
#include <stdlib.h>

#include "data.hpp"

#if DATA_TYPE == MNIST

#define NAME_TRAIN_FILE_IMAGE "../data/train-images-idx3-ubyte"
#define NAME_TRAIN_FILE_LABEL "../data/train-labels-idx1-ubyte"
#define SIZE_TRAIN_FILE_IMAGE 47040016
#define SIZE_TRAIN_FILE_LABEL 60008

#define NAME_TEST_FILE_IMAGE "../data/t10k-images-idx3-ubyte"
#define NAME_TEST_FILE_LABEL "../data/t10k-labels-idx1-ubyte"
#define SIZE_TEST_FILE_IMAGE 7840016
#define SIZE_TEST_FILE_LABEL 10008

inline static void read_file(const char* file_name,
                             unsigned char* buffer,
                             size_t buffer_size) {
  FILE *ptr;
  ptr = fopen(file_name, "rb"); // r for read, b for binary
  fread(buffer, buffer_size, 1, ptr); // read 10 bytes to our buffer
}

inline static dataset_t get_dataset(const int n,
                                    const int dim,
                                    const int num_labels,
                                    const char* image_file,
                                    const int img_filesize,
                                    const char* lab_file,
                                    const int lab_filesize) {
  dataset_t ret(n, dim, num_labels);

  unsigned char lab_buffer[lab_filesize];
  read_file(lab_file, lab_buffer, lab_filesize);
  int lab_offset = 8;
  for (int i = lab_offset; i < lab_filesize; i++) {
    int lab_index = i - lab_offset;

    ret.labels_idx[lab_index] = lab_buffer[i];
    for(int j = 0; j < num_labels; j++){
      float lab = lab_buffer[i] == j? 1 : 0;
      ret.labels_oh[lab_index * num_labels + j] = lab;
    }
  }

  unsigned char* img_buffer = (unsigned char*) calloc(img_filesize, sizeof(unsigned char));
  read_file(image_file, img_buffer, img_filesize);
  int img_offset = 16;
  for (int i = img_offset; i < img_filesize; i++) {
    ret.image[i - img_offset] = (float) img_buffer[i] / 255;
  }
  free(img_buffer);

  return ret;
}

dataset_t get_train_dataset() {
  return get_dataset(60000, 28 * 28, 10,
                     NAME_TRAIN_FILE_IMAGE,
                     SIZE_TRAIN_FILE_IMAGE,
                     NAME_TRAIN_FILE_LABEL,
                     SIZE_TRAIN_FILE_LABEL);
}

dataset_t get_test_dataset() {
  return get_dataset(60000, 28 * 28, 10,
                     NAME_TEST_FILE_IMAGE,
                     SIZE_TEST_FILE_IMAGE,
                     NAME_TEST_FILE_LABEL,
                     SIZE_TEST_FILE_LABEL);
}

#endif /* DATA_TYPE */
