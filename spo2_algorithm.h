#ifndef SPO2_ALGORITHM_H_
#define SPO2_ALGORITHM_H_

#include <Arduino.h>

#define FreqS 25    //sampling frequency
#define BUFFER_SIZE (FreqS * 4) 
#define MA4_SIZE 4 // DONOT CHANGE

const uint8_t uch_spo2_table[184] = { 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99, 
  99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 
  100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 
  97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 
  90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 
  80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 
  66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 
  49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29, 
  28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5, 
  3, 2, 1 } ;

#ifdef __cplusplus
extern "C" {
#endif

void maxim_heart_rate_and_oxygen_saturation(
  uint32_t *irBuffer,
  uint32_t *redBuffer,
  int32_t bufferLength,
  int32_t *spo2,
  int8_t *spo2Valid,
  int32_t *heartRate,
  int8_t *hrValid);

void maxim_find_peaks(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t size, int32_t minHeight, int32_t minDist, int32_t maxNum);
void maxim_peaks_above_min_height(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t size, int32_t minHeight);
void maxim_remove_close_peaks(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t minDist);
void maxim_sort_ascend(int32_t *data, int32_t size);
void maxim_sort_indices_descend(int32_t *data, int32_t *index, int32_t size);

#ifdef __cplusplus
}
#endif

#endif
