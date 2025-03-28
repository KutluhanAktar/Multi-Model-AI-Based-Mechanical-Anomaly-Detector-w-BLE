         /////////////////////////////////////////////  
        //   Multi-Model AI-Based Mechanical       //
       //        Anomaly Detector w/ BLE          //
      //             ---------------             //
     //           (Beetle ESP32 - C3)           //           
    //             by Kutluhan Aktar           // 
   //                                         //
  /////////////////////////////////////////////

//
// Apply sound data-based anomalous behavior detection, diagnose the root cause via object detection concurrently, and inform the user via SMS.
//
// For more information:
// https://www.theamplituhedron.com/projects/Multi_Model_AI_Based_Mechanical_Anomaly_Detector
//
//
// Connections
// Beetle ESP32 - C3 :
//                                Fermion: I2S MEMS Microphone
// 3.3V    ------------------------ 3v3
// 0       ------------------------ WS
// GND     ------------------------ SEL
// 1       ------------------------ SCK
// 4       ------------------------ DO
//                                Long-Shaft Linear Potentiometer 
// 2       ------------------------ S
//                                Control Button (A)
// 8       ------------------------ +
//                                Control Button (B)
// 9       ------------------------ +
//                                5mm Common Anode RGB LED
// 5       ------------------------ R
// 6       ------------------------ G
// 7       ------------------------ B
//                                FireBeetle 2 ESP32-S3
// RX (20) ------------------------ TX (43) 
// TX (21) ------------------------ RX (44)


// Include the required libraries:
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <driver/i2s.h>

// Include the Edge Impulse neural network model converted to an Arduino library:
#include <AI-Based_Mechanical_Anomaly_Detector_Audio__inferencing.h>

// Define the required parameters to run an inference with the Edge Impulse neural network model.
#define sample_buffer_size 512
int16_t sampleBuffer[sample_buffer_size];

// Define the threshold value for the model outputs (predictions).
float threshold = 0.60;

// Define the anomaly class names:
String classes[] = {"anomaly", "normal"};

// Create the BLE service:
BLEService Anomaly_Detector("e1bada10-a728-44c6-a577-6f9c24fe980a");

// Create data characteristics and allow the remote device (central) to write, read, and notify:
BLEFloatCharacteristic audio_detected_Characteristic("e1bada10-a728-44c6-a577-6f9c24fe984a", BLERead | BLENotify);
BLEByteCharacteristic selected_img_class_Characteristic("e1bada10-a728-44c6-a577-6f9c24fe981a", BLERead | BLEWrite);
BLEByteCharacteristic given_command_Characteristic("e1bada10-a728-44c6-a577-6f9c24fe983a", BLERead | BLEWrite);
 
// Define the Fermion I2S MEMS microphone configurations.
#define I2S_SCK    1
#define I2S_WS     0
#define I2S_SD     4
#define DATA_BIT   (16) //16-bit
// Define the I2S processor port.
#define I2S_PORT I2S_NUM_0

// Define the potentiometer settings.
#define potentiometer_pin 2

// Define the RGB pin settings.
#define red_pin 5
#define green_pin 6
#define blue_pin 7

// Define the control buttons.
#define control_button_1 8
#define control_button_2 9

// Define the data holders:
volatile boolean _connected = false;
int predicted_class = -1;
int pre_pot_value = 0, selected_img_class, audio_model_interval = 80;
long timer;

void setup(){
  Serial.begin(115200);
  // Initiate the serial communication between Beetle ESP32 - C3 and FireBeetle 2 ESP32-S3.
  Serial1.begin(9600, SERIAL_8N1, /*RX=*/20, /*TX=*/21);  

  pinMode(red_pin, OUTPUT); pinMode(green_pin, OUTPUT); pinMode(blue_pin, OUTPUT);
  digitalWrite(red_pin, HIGH); digitalWrite(green_pin, HIGH); digitalWrite(blue_pin, HIGH);

  pinMode(control_button_1, INPUT_PULLUP);
  pinMode(control_button_2, INPUT_PULLUP);
 
  // Configure the I2S port for the I2S microphone.
  i2s_install(EI_CLASSIFIER_FREQUENCY);
  i2s_setpin();
  i2s_start(I2S_PORT);
  delay(1000);

  // Check the BLE initialization status:
  while(!BLE.begin()){
    Serial.println("BLE initialization is failed!");
  }
  Serial.println("\nBLE initialization is successful!\n");
  // Print this peripheral device's address information:
  Serial.print("MAC Address: "); Serial.println(BLE.address());
  Serial.print("Service UUID Address: "); Serial.println(Anomaly_Detector.uuid()); Serial.println();

  // Set the local name this peripheral advertises: 
  BLE.setLocalName("BLE Anomaly Detector");
  // Set the UUID for the service this peripheral advertises:
  BLE.setAdvertisedService(Anomaly_Detector);

  // Add the given data characteristics to the service:
  Anomaly_Detector.addCharacteristic(audio_detected_Characteristic);
  Anomaly_Detector.addCharacteristic(selected_img_class_Characteristic);
  Anomaly_Detector.addCharacteristic(given_command_Characteristic);

  // Add the given service to the advertising device:
  BLE.addService(Anomaly_Detector);

  // Assign event handlers for connected and disconnected devices to/from this peripheral:
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // Assign event handlers for the data characteristics modified (written) by the central device (via the Android application).
  // In this regard, obtain the transferred (written) data packets from the Android application over BLE.
  selected_img_class_Characteristic.setEventHandler(BLEWritten, get_central_BLE_updates);
  given_command_Characteristic.setEventHandler(BLEWritten, get_central_BLE_updates);

  // Start advertising:
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");

  delay(5000);

  // Update the timer.
  timer = millis();
}
 
void loop(){
  // Poll for BLE events:
  BLE.poll();

  // If the potentiometer value is altered, change the selected image class for data collection manually.
  int current_pot_value = map(analogRead(potentiometer_pin), 360, 4096, 0, 10); 
  delay(100);
  if(abs(current_pot_value-pre_pot_value) > 1){
    if(current_pot_value == 0){ adjustColor(true, true, true); }
    if(current_pot_value > 0 && current_pot_value <= 3){ adjustColor(true, false, false); selected_img_class = 0; }
    if(current_pot_value > 3 && current_pot_value <= 7){ adjustColor(false, true, false); selected_img_class = 1; }
    if(current_pot_value > 7){ adjustColor(false, false, true); selected_img_class = 2; }
    pre_pot_value = current_pot_value;
  }

  // If the control button (A) is pressed, transfer the given image class (manually or via BLE) to FireBeetle 2 ESP32-S3 via serial communication.
  if(!digitalRead(control_button_1)){ Serial1.print("IMG_Class=" + String(selected_img_class)); delay(500); adjustColor(false, true, true); }

  // If the control button (B) is pressed, force FireBeetle 2 ESP32-S3 to run the object detection model despite not detecting a mechanical anomaly via the neural network model (microphone).
  if(!digitalRead(control_button_2)){ Serial1.print("Run Inference"); delay(500); adjustColor(true, false, true); }

  // Depending on the configured model interval (default 80 seconds) via the Android application, run the Edge Impulse neural network model to detect mechanical anomalies via the I2S microphone.
  if(millis() - timer > audio_model_interval*1000){
    // Run inference.
    run_inference_to_make_predictions();
    // If the Edge Impulse neural network model detects a mechanical anomaly successfully:
    if(predicted_class > -1){
      // Update the audio detection characteristic via BLE.
      update_characteristics(predicted_class);
      delay(2000);
      // Make FireBeetle 2 ESP32-S3 to run the object detection model to diagnose the root cause of the detected mechanical anomaly.
      if(classes[predicted_class] == "anomaly") Serial1.print("Run Inference"); delay(500); adjustColor(true, false, true);
      // Clear the predicted class (label).
      predicted_class = -1;
      }
      // Update the timer:
      timer = millis();
    }  
}

void run_inference_to_make_predictions(){
  // Summarize the Edge Impulse neural network model inference settings (from model_metadata.h):
  ei_printf("\nInference settings:\n");
  ei_printf("\tInterval: "); ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS); ei_printf(" ms.\n");
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  // If the I2S microphone generates an audio (data) buffer successfully:
  bool sample = microphone_sample(2000);
  if(sample){
    // Run inference:
    ei::signal_t signal;
    // Create a signal object from the resized (scaled) audio buffer.
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    // Run the classifier:
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR _err = run_classifier(&signal, &result, false);
    if(_err != EI_IMPULSE_OK){
      ei_printf("ERR: Failed to run classifier (%d)\n", _err);
      return;
    }

    // Print the inference timings on the serial monitor.
    ei_printf("\nPredictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);

    // Obtain the prediction results for each label (class).
    for(size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++){
      // Print the prediction results on the serial monitor.
      ei_printf("%s:\t%.5f\n", result.classification[ix].label, result.classification[ix].value);
      // Get the imperative predicted label (class).
      if(result.classification[ix].value >= threshold) predicted_class = ix;
    }
    ei_printf("\nPredicted Class: %d [%s]\n", predicted_class, classes[predicted_class]);  

    // Detect anomalies, if any:
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
      ei_printf("Anomaly: ");
      ei_printf_float(result.anomaly);
      ei_printf("\n");
    #endif 

    // Release the audio buffer.
    //ei_free(sampleBuffer);
  }
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr){
  // Convert the given microphone (audio) data (buffer) to the out_ptr format required by the Edge Impulse neural network model.
  numpy::int16_to_float(&sampleBuffer[offset], out_ptr, length);
  return 0;
}

bool microphone_sample(int range){
  // Display the collected audio data according to the given range (sensitivity).
  // Serial.print(range * -1); Serial.print(" "); Serial.print(range); Serial.print(" ");
 
  // Obtain the information generated by the I2S microphone and save it to the input buffer — sampleBuffer.
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sampleBuffer, sample_buffer_size, &bytesIn, portMAX_DELAY);

  // If the I2S microphone generates audio data successfully:
  if(result == ESP_OK){
    Serial.println("\nAudio Data Generated Successfully!");
    
    // Depending on the given model, scale (resize) the collected audio buffer (data) by the I2S microphone. Otherwise, the sound might be too quiet.
    for(int x = 0; x < bytesIn/2; x++) {
      sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
    }
      
    /*
    // Display the average audio data reading on the serial plotter.
    int16_t samples_read = bytesIn / 8;
    if(samples_read > 0){
      float mean = 0;
      for(int16_t i = 0; i < samples_read; ++i){ mean += (sampleBuffer[i]); }
      mean /= samples_read;
      Serial.println(mean);
    }
    */

    return true;
  }else{
    Serial.println("\nAudio Data Failed!");
    return false;
  }
}

void update_characteristics(float detection){
  // Update the selected characteristics over BLE.
  audio_detected_Characteristic.writeValue(detection);
  Serial.println("\n\nBLE: Data Characteristics Updated Successfully!\n");
}

void get_central_BLE_updates(BLEDevice central, BLECharacteristic characteristic){
  delay(500);
  // Obtain the recently transferred data packets from the central device over BLE.
  if(characteristic.uuid() == selected_img_class_Characteristic.uuid()){
    // Get the given image class for data collection.
    selected_img_class = selected_img_class_Characteristic.value();
    if(selected_img_class == 0) adjustColor(true, false, false);
    if(selected_img_class == 1) adjustColor(false, true, false);
    if(selected_img_class == 2) adjustColor(false, false, true);
    Serial.print("\nSelected Image Data Class (BLE) => "); Serial.println(selected_img_class);
    // Transfer the passed image class to FireBeetle 2 ESP32-S3 via serial communication.
    Serial1.print("IMG_Class=" + String(selected_img_class)); delay(500);
  }
  if(characteristic.uuid() == given_command_Characteristic.uuid()){
    int command = given_command_Characteristic.value();
    // Change the interval for running the neural network model (microphone) to detect mechanical anomalies.
    if(command < 130){
      audio_model_interval = command;
      Serial.print("\nGiven Model Interval (Audio) => "); Serial.println(audio_model_interval);
    // Force FireBeetle 2 ESP32-S3 to run the object detection model despite not detecting a mechanical anomaly via the neural network model (microphone).
    }else if(command > 130){
      Serial1.print("Run Inference"); delay(500); adjustColor(true, false, true);
    }
  }
}

void i2s_install(uint32_t sampling_rate){
  // Configure the I2S processor port for the I2S microphone (ONLY_LEFT).
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = sampling_rate,
    .bits_per_sample = i2s_bits_per_sample_t(DATA_BIT),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = sample_buffer_size,
    .use_apll = false
  };
 
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}
 
void i2s_setpin(){
  // Set the I2S microphone pin configuration.
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
 
  i2s_set_pin(I2S_PORT, &pin_config);
}

void blePeripheralConnectHandler(BLEDevice central){
  // Central connected event handler:
  Serial.print("\nConnected event, central: ");
  Serial.println(central.address());
  _connected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central){
  // Central disconnected event handler:
  Serial.print("\nDisconnected event, central: ");
  Serial.println(central.address());
  _connected = false;
}

void adjustColor(boolean r, boolean g, boolean b){
  if(r){ digitalWrite(red_pin, LOW); }else{digitalWrite(red_pin, HIGH);}
  if(g){ digitalWrite(green_pin, LOW); }else{digitalWrite(green_pin, HIGH);}
  if(b){ digitalWrite(blue_pin, LOW); }else{digitalWrite(blue_pin, HIGH);}
}
