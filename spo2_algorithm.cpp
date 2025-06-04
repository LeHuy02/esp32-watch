#include "Arduino.h"
#include "spo2_algorithm.h"

static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];

void maxim_heart_rate_and_oxygen_saturation(uint32_t *irBuffer, uint32_t *redBuffer, int32_t bufferLength,
                                            int32_t *spo2, int8_t *spo2Valid, int32_t *heartRate, int8_t *hrValid) {
  uint32_t irMean = 0;
  int32_t npks = 0, peakIntervalSum = 0;
  int32_t valleyLocs[15] = {0};
  for (int i = 0; i < bufferLength; i++) irMean += irBuffer[i];
  irMean /= bufferLength;

  for (int i = 0; i < bufferLength; i++) an_x[i] = -1 * (irBuffer[i] - irMean);
  for (int i = 0; i < BUFFER_SIZE - MA4_SIZE; i++)
    an_x[i] = (an_x[i] + an_x[i + 1] + an_x[i + 2] + an_x[i + 3]) / 4;

  int32_t threshold = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) threshold += an_x[i];
  threshold = constrain(threshold / BUFFER_SIZE, 30, 60);

  maxim_find_peaks(valleyLocs, &npks, an_x, BUFFER_SIZE, threshold, 4, 15);

  if (npks >= 2) {
    for (int i = 1; i < npks; i++) peakIntervalSum += (valleyLocs[i] - valleyLocs[i - 1]);
    peakIntervalSum /= (npks - 1);
    *heartRate = (FreqS * 60) / peakIntervalSum;
    *hrValid = 1;
  } else {
    *heartRate = -999;
    *hrValid = 0;
  }

  for (int i = 0; i < bufferLength; i++) {
    an_x[i] = irBuffer[i];
    an_y[i] = redBuffer[i];
  }

  int32_t ratios[5] = {0}, ratioCount = 0;
  for (int k = 0; k < npks - 1 && ratioCount < 5; k++) {
    int32_t irMin = valleyLocs[k], irMax = valleyLocs[k + 1];
    if (irMax >= BUFFER_SIZE) continue;

    int32_t yAc = an_y[irMax] - an_y[irMin];
    int32_t xAc = an_x[irMax] - an_x[irMin];
    int32_t yDc = an_y[irMin];
    int32_t xDc = an_x[irMin];
    if (xAc != 0 && yDc != 0) {
      ratios[ratioCount++] = ((yAc * xDc) << 7) / (xAc * yDc);
    }
  }

  if (ratioCount > 0) {
    maxim_sort_ascend(ratios, ratioCount);
    int ratioAvg = ratios[ratioCount / 2];
    if (ratioAvg >= 2 && ratioAvg < 184) {
      *spo2 = uch_spo2_table[ratioAvg];
      *spo2Valid = 1;
    } else {
      *spo2 = -999;
      *spo2Valid = 0;
    }
  } else {
    *spo2 = -999;
    *spo2Valid = 0;
  }
}

void maxim_find_peaks(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t size, int32_t minHeight, int32_t minDist, int32_t maxNum) {
  maxim_peaks_above_min_height(locs, numPeaks, data, size, minHeight);
  maxim_remove_close_peaks(locs, numPeaks, data, minDist);
  *numPeaks = min(*numPeaks, maxNum);
}

void maxim_peaks_above_min_height(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t size, int32_t minHeight) {
  int32_t i = 1, width;
  *numPeaks = 0;
  while (i < size - 1) {
    if (data[i] > minHeight && data[i] > data[i - 1]) {
      width = 1;
      while (i + width < size && data[i] == data[i + width]) width++;
      if (data[i] > data[i + width]) {
        locs[(*numPeaks)++] = i;
        i += width + 1;
      } else i += width;
    } else i++;
  }
}

void maxim_remove_close_peaks(int32_t *locs, int32_t *numPeaks, int32_t *data, int32_t minDist) {
  maxim_sort_indices_descend(data, locs, *numPeaks);
  int32_t i, j, dist, oldNum = *numPeaks;
  *numPeaks = 0;
  for (i = 0; i < oldNum; i++) {
    bool keep = true;
    for (j = 0; j < *numPeaks; j++) {
      dist = abs(locs[i] - locs[j]);
      if (dist < minDist) { keep = false; break; }
    }
    if (keep) locs[(*numPeaks)++] = locs[i];
  }
  maxim_sort_ascend(locs, *numPeaks);
}

void maxim_sort_ascend(int32_t *data, int32_t size) {
  for (int i = 1; i < size; i++) {
    int32_t temp = data[i], j = i;
    while (j > 0 && temp < data[j - 1]) { data[j] = data[j - 1]; j--; }
    data[j] = temp;
  }
}

void maxim_sort_indices_descend(int32_t *data, int32_t *index, int32_t size) {
  for (int i = 1; i < size; i++) {
    int32_t temp = index[i], j = i;
    while (j > 0 && data[temp] > data[index[j - 1]]) { index[j] = index[j - 1]; j--; }
    index[j] = temp;
  }
}
