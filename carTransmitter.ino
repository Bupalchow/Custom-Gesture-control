#include <CarTransmitter_inferencing.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SoftwareSerial.h>

// Initialize MPU6050 and SoftwareSerial
Adafruit_MPU6050 mpu;
SoftwareSerial BTSerial(0, 2); // RX, TX pins for HC-05 (D3 -> GPIO 0, D4 -> GPIO 2)

// Buffer to store accelerometer data
float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// Function to provide data to the classifier
int get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, buffer + offset, length * sizeof(float));
  return 0;
}

// Define function to classify gestures using Edge Impulse model
int classifyGesture(float x, float y, float z) {
  signal_t signal;
  ei_impulse_result_t result = {0};

  // Fill the buffer with accelerometer data
  buffer[0] = x;
  buffer[1] = y;
  buffer[2] = z;

  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &get_data;

  EI_IMPULSE_ERROR ei_error = run_classifier(&signal, &result, false);
  if (ei_error != EI_IMPULSE_OK) {
    Serial.print("ERR: ");
    Serial.println(ei_error);
    return -1;
  }

  // Print the classification results
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.print(result.classification[ix].label);
    Serial.print(": ");
    Serial.println(result.classification[ix].value);
  }

  // Find the gesture with the highest probability
  float max_probability = 0.0;
  int max_index = -1;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (result.classification[ix].value > max_probability) {
      max_probability = result.classification[ix].value;
      max_index = ix;
    }
  }

  // Check if the maximum probability is above 60%
  if (max_probability >= 0.75) {
    Serial.print("Gesture with sufficient probability detected: ");
    Serial.print(result.classification[max_index].label);
    Serial.print(" with probability ");
    Serial.println(max_probability);
    return max_index;
  } else {
    Serial.println("No gesture detected with sufficient probability.");
    return -1; // Indicate no confident classification
  }
}

void setup() {
  Serial.begin(115200);   // For debugging
  BTSerial.begin(9600);  // Default speed of HC-05

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  } else {
    Serial.println("MPU6050 initialized successfully");
  }

  // Set up accelerometer range
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  // Set up gyroscope range
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  // Set up filter bandwidth
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 setup complete!");
  delay(100);

  // Initialize the Edge Impulse model
  run_classifier_init();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Print accelerometer data for debugging
  Serial.print("X: ");
  Serial.print(a.acceleration.x);
  Serial.print(" Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(" Z: ");
  Serial.println(a.acceleration.z);

  // Classify gestures using Edge Impulse model
  int gesture_index = classifyGesture(a.acceleration.x, a.acceleration.y, a.acceleration.z);
  char command = 'S'; // Default command to 'Stop'

  // Process Edge Impulse model results
  if (gesture_index >= 0) {
    switch (gesture_index) {
      case 3:
        command = 'C'; // Circle
        break;
      case 4:
        command = 'K'; // Shake
        break;
      default:
        command = 'S'; // No valid gesture detected
        break;
    }
  }

  // Process other gestures based on tilt only if no valid gesture was detected
  if (command == 'S') {
    command = getTiltDirection(a.acceleration.x, a.acceleration.y, a.acceleration.z);
  }
  if (command != 'S') { // Send command only if it's not 'Stop'
    BTSerial.write(command);
    Serial.print("Command sent: ");
    Serial.println(command); // For debugging
  } else {
    Serial.println("No command sent");
  }

  delay(100); // Adjust the delay as needed
}

char getTiltDirection(float x, float y, float z) {
  // Define thresholds for detecting tilt
  float forwardThreshold = 5.0;
  float backwardThreshold = -5.0;
  float rightThreshold = 5.0;
  float leftThreshold = -5.0;

  if (y > forwardThreshold) {
    return 'L'; // Forward
  } else if (y < backwardThreshold) {
    return 'R'; // Backward
  } else if (x > rightThreshold) {
    return 'B'; // Right
  } else if (x < leftThreshold) {
    return 'F'; // Left
  } else {
    return 'S'; // Stop
  }
}
