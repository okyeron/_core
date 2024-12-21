#include <stddef.h>
#include <stdint.h>

#define MEDIAN_FILTER_SIZE 7
#define FIXED_POINT_SCALE \
  256  // Scale factor for fixed-point arithmetic (e.g., 8 bits for fraction)

// Structure for filtering knob values
typedef struct KnobFilter {
  uint16_t buffer[MEDIAN_FILTER_SIZE];  // Buffer for median filter
  size_t index;                         // Current index in the buffer
  uint16_t filtered_value;              // Last filtered value
  uint16_t low_pass_alpha;              // Fixed-point alpha for low-pass filter
  uint16_t last_value;
} KnobFilter;

// Helper function: Sort an array and return the median value
static uint16_t calculate_median(uint16_t *values, size_t size) {
  uint16_t sorted[size];
  // Copy values into a temporary array
  for (size_t i = 0; i < size; i++) {
    sorted[i] = values[i];
  }
  // Sort the temporary array
  for (size_t i = 0; i < size - 1; i++) {
    for (size_t j = i + 1; j < size; j++) {
      if (sorted[i] > sorted[j]) {
        uint16_t temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }
  // Return the median value
  return sorted[size / 2];
}

// Initialize the filter
void knob_filter_init(KnobFilter *filter, uint8_t alpha) {
  for (size_t i = 0; i < MEDIAN_FILTER_SIZE; i++) {
    filter->buffer[i] = 0;
  }
  filter->last_value = 0;
  filter->index = 0;
  filter->filtered_value = 0;
  // Convert alpha (0-255) to fixed-point representation
  filter->low_pass_alpha = (uint16_t)(alpha * FIXED_POINT_SCALE / 255);
}

// Update the filter with a new raw value and get the filtered value
int16_t knob_filter_update(KnobFilter *filter, uint16_t new_value) {
  // Add the new value to the buffer
  filter->buffer[filter->index] = new_value;
  filter->index = (filter->index + 1) % MEDIAN_FILTER_SIZE;

  // Calculate the median of the buffer
  uint16_t median = calculate_median(filter->buffer, MEDIAN_FILTER_SIZE);

  // Apply the low-pass filter using fixed-point math:
  // filtered_value = alpha * median + (1 - alpha) * filtered_value
  filter->filtered_value =
      (uint16_t)(((filter->low_pass_alpha * median) +
                  ((FIXED_POINT_SCALE - filter->low_pass_alpha) *
                   filter->filtered_value)) /
                 FIXED_POINT_SCALE);

  if (filter->filtered_value == filter->last_value) {
    return -1;
  }
  filter->last_value = filter->filtered_value;
  return filter->filtered_value;
}