         /////////////////////////////////////////////  
        //   Multi-Model AI-Based Mechanical       //
       //        Anomaly Detector w/ BLE          //
      //             ---------------             //
     //         (FireBeetle 2 ESP32-S3)         //           
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
// FireBeetle 2 ESP32-S3 :
//                                Fermion 2.0" IPS TFT LCD Display (320x240)
// 3.3V    ------------------------ V
// 17/SCK  ------------------------ CK
// 15/MOSI ------------------------ SI
// 16/MISO ------------------------ SO
// 18/D6   ------------------------ CS
// 38/D3   ------------------------ RT
// 3/D2    ------------------------ DC
// 21/D13  ------------------------ BL
// 9/D7    ------------------------ SC
//                                Beetle ESP32 - C3
// RX (44) ------------------------ TX (21)
// TX (43) ------------------------ RX (20)


// Include the required libraries:
#include <WiFi.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "DFRobot_GDL.h"
#include "DFRobot_Picdecoder_SD.h"

// Add the icons to be shown on the Fermion TFT LCD display.
#include "logo.h"

// Include the Edge Impulse FOMO model converted to an Arduino library:
#include <AI-Based_Mechanical_Anomaly_Detector_Camera__inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

// Define the required parameters to run an inference with the Edge Impulse FOMO model.
#define CAPTURED_IMAGE_BUFFER_COLS        240
#define CAPTURED_IMAGE_BUFFER_ROWS        240
#define EI_CAMERA_FRAME_BYTE_SIZE         3
uint8_t *ei_camera_capture_out;

// Define the component (part) class names:
String classes[] = {"red", "green", "blue"};

char ssid[] = "<__SSID__>";      // your network SSID (name)
char pass[] = "<__PASSWORD__>";  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)

// Define the server on LattePanda 3 Delta 864.
char server[] = "192.168.1.22";
// Define the web application path.
String application = "/mechanical_anomaly_detector/update.php";

// Initialize the WiFiClient object.
WiFiClient client; /* WiFiSSLClient client; */

// Define the onboard OV2640 camera pin configurations.
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     45
#define SIOD_GPIO_NUM     1
#define SIOC_GPIO_NUM     2

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       46
#define Y7_GPIO_NUM       8
#define Y6_GPIO_NUM       7
#define Y5_GPIO_NUM       4
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     42
#define PCLK_GPIO_NUM     5

// Since FireBeetle 2 ESP32-S3 has an independent camera power supply circuit, enable the AXP313A power output when using the camera.
#include "DFRobot_AXP313A.h"
DFRobot_AXP313A axp;

// Utilize the built-in MicroSD card reader on the Fermion 2.0" TFT LCD display (320x240).
#define SD_CS_PIN D7

// Define the Fermion TFT LCD display pin configurations.
#define TFT_DC  D2
#define TFT_CS  D6
#define TFT_RST D3

// Define the Fermion TFT LCD display object and integrated JPG decoder.
DFRobot_Picdecoder_SD decoder;
DFRobot_ST7789_240x320_HW_SPI screen(/*dc=*/TFT_DC,/*cs=*/TFT_CS,/*rst=*/TFT_RST);

// Create a struct (_data) including all resulting bounding box parameters:
struct _data {
  String x;
  String y;
  String w;
  String h;
};

// Define the data holders:
uint16_t class_color[3] = {COLOR_RGB565_RED, COLOR_RGB565_GREEN, COLOR_RGB565_BLUE};
volatile boolean s_init = true;
String data_packet = "";
int sample_number[3] = {0, 0, 0};
int predicted_class = -1;
struct _data box;

void setup() {
  Serial.begin(115200);
  // Initiate the serial communication between FireBeetle 2 ESP32-S3 and Beetle ESP32 - C3.
  Serial1.begin(9600, SERIAL_8N1, /*RX=*/44,/*TX=*/43);

  // Enable the independent camera power supply circuit (AXP313A) for the built-in OV2640 camera.
  while(axp.begin() != 0){
    Serial.println("Camera power init failed!");
    delay(1000);
  }
  axp.enableCameraPower(axp.eOV2640);

  // Define the OV2640 camera pin and frame settings.
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;          // Set XCLK_FREQ_HZ as 10KHz to avoid the EV-VSYNC-OVF error.
  config.frame_size = FRAMESIZE_240X240;   // FRAMESIZE_QVGA (320x240), FRAMESIZE_SVGA
  config.pixel_format = PIXFORMAT_RGB565;  // PIXFORMAT_JPEG
  config.grab_mode = CAMERA_GRAB_LATEST;   // CAMERA_GRAB_WHEN_EMPTY 
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;                     // for CONFIG_IDF_TARGET_ESP32S3   

  // Initialize the OV2640 camera.
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Initialize the Fermion TFT LCD display. 
  screen.begin();
  screen.setRotation(2);
  delay(1000);

  // Initialize the MicroSD card module on the Fermion TFT LCD display.
  while(!SD.begin(SD_CS_PIN)){
    Serial.println("SD Card => No module found!");
    delay(200);
    return;
  }

  // Connect to WPA/WPA2 network. Change this line if using an open or WEP network.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  // Attempt to connect to the given Wi-Fi network.
  while(WiFi.status() != WL_CONNECTED){
    // Wait for the network connection.
    delay(500);
    Serial.print(".");
  }
  // If connected to the network successfully:
  Serial.println("Connected to the Wi-Fi network successfully!");
}

void loop(){
  // Display the initialization screen.
  if(s_init){
    screen.fillScreen(COLOR_RGB565_BLACK);
    screen.drawXBitmap(/*x=*/(240-iron_giant_width)/2,/*y=*/(320-iron_giant_height)/2,/*bitmap gImage_Bitmap=*/iron_giant_bits,/*w=*/iron_giant_width,/*h=*/iron_giant_height,/*color=*/COLOR_RGB565_PURPLE);
    delay(1000);
  } s_init = false;

  
  // Obtain the data packet transferred by Beetle ESP32 - C3 via serial communication.
  if(Serial1.available() > 0){
    data_packet = Serial1.readString();
  }

  if(data_packet != ""){
    Serial.println("\nReceived Data Packet => " + data_packet);
    // If Beetle ESP32 - C3 transfers a component (part) class via serial communication:
    if(data_packet.indexOf("IMG_Class") > -1){
      // Decode the received data packet to elicit the passed class.
      int delimiter_1 = data_packet.indexOf("=");
      // Glean information as substrings.
      String s = data_packet.substring(delimiter_1 + 1);
      int given_class = s.toInt();
      // Capture a new frame (RGB565 buffer) with the OV2640 camera.
      camera_fb_t *fb = esp_camera_fb_get();
      if(!fb){ Serial.println("Camera => Cannot capture the frame!"); return; }
      // Convert the captured RGB565 buffer to JPEG buffer.
      size_t con_len;
      uint8_t *con_buf = NULL;
      if(!frame2jpg(fb, 10, &con_buf, &con_len)){ Serial.println("Camera => Cannot convert the RGB565 buffer to JPEG!"); return; }
      delay(500);
      // Depending on the given component (part) class, save the converted frame as a sample to the SD card.
      String file_name = "";
      file_name = "/" + classes[given_class] + "_" + String(sample_number[given_class]) + ".jpg";
      // After defining the file name by adding the sample number, save the converted frame to the SD card.
      if(save_image(SD, file_name.c_str(), con_buf, con_len)){
        screen.fillScreen(COLOR_RGB565_BLACK);
        screen.setTextColor(class_color[given_class]);
        screen.setTextSize(2);
        // Display the assigned class icon.
        screen.drawXBitmap(/*x=*/10,/*y=*/250,/*bitmap gImage_Bitmap=*/save_bits,/*w=*/save_width,/*h=*/save_height,/*color=*/class_color[given_class]);
        screen.setCursor(20+save_width, 255);
        screen.println("IMG Saved =>");
        screen.setCursor(20+save_width, 275);
        screen.println(file_name);
        delay(1000);
        // Increase the sample number of the given class.
        sample_number[given_class]+=1;
        Serial.println("\nImage Sample Saved => " + file_name);
        // Draw the recently saved image sample on the screen to notify the user.
        decoder.drawPicture(/*filename=*/file_name.c_str(),/*sx=*/0,/*sy=*/0,/*ex=*/240,/*ey=*/240,/*screenDrawPixel=*/screenDrawPixel);
        delay(1000);
      }else{
        screen.fillScreen(COLOR_RGB565_BLACK);
        screen.setTextColor(class_color[given_class]);
        screen.setTextSize(2);
        screen.drawXBitmap(/*x=*/10,/*y=*/250,/*bitmap gImage_Bitmap=*/save_bits,/*w=*/save_width,/*h=*/save_height,/*color=*/class_color[given_class]);
        screen.setCursor(20+save_width, 255);
        screen.println("SD Card =>");
        screen.setCursor(20+save_width, 275);
        screen.println("File Error!");
        delay(1000);
      }
      // Release the image buffers.
      free(con_buf);
      esp_camera_fb_return(fb); 
    // If requested, run the Edge Impulse FOMO model to make predictions on the root cause of the detected mechanical anomaly.       
    }else if(data_packet.indexOf("Run") > -1){
      // Capture a new frame (RGB565 buffer) with the OV2640 camera.
      camera_fb_t *fb = esp_camera_fb_get();
      if(!fb){ Serial.println("Camera => Cannot capture the frame!"); return; }
      // Run inference.
      run_inference_to_make_predictions(fb);
      // If the Edge Impulse FOMO model detects a component (part) class successfully:
      if(predicted_class > -1){
        // Define the query parameters, including the passed bounding box measurements.
        String query = "?results=OK&class=" + classes[predicted_class]
                     + "&x=" + box.x
                     + "&y=" + box.y
                     + "&w=" + box.w
                     + "&h=" + box.h;
        // Make an HTTP POST request to the given web application so as to transfer the model results, including the resulting image and the bounding box measurements.
        make_a_post_request(fb, query);
        // Notify the user of the detected component (part) class on the Fermion TFT LCD display.
        screen.fillScreen(COLOR_RGB565_BLACK);
        screen.setTextColor(class_color[predicted_class]);
        screen.setTextSize(4);
        // Display the gear icon with the assigned class color.
        screen.drawXBitmap(/*x=*/(240-gear_width)/2,/*y=*/(320-gear_height)/2,/*bitmap gImage_Bitmap=*/gear_bits,/*w=*/gear_width,/*h=*/gear_height,/*color=*/class_color[predicted_class]);
        screen.setCursor((240-(classes[predicted_class].length()*20))/2, ((320-gear_height)/2)+gear_height+30);
        String t = classes[predicted_class];
        t.toUpperCase();
        screen.println(t);
        delay(3000);                    
        // Clear the predicted class (label).
        predicted_class = -1;
      }else{
        screen.fillScreen(COLOR_RGB565_BLACK);
        screen.setTextColor(COLOR_RGB565_WHITE);
        screen.drawXBitmap(/*x=*/(240-gear_width)/2,/*y=*/(320-gear_height)/2,/*bitmap gImage_Bitmap=*/gear_bits,/*w=*/gear_width,/*h=*/gear_height,/*color=*/COLOR_RGB565_WHITE);
        delay(3000);            
      }
      // Release the image buffer.
      esp_camera_fb_return(fb);
    }
    // Clear the received data packet.
    data_packet = "";
    // Return to the initialization screen.
    delay(500);
    s_init = true;
  }
}

void run_inference_to_make_predictions(camera_fb_t *fb){
  // Summarize the Edge Impulse FOMO model inference settings (from model_metadata.h):
  ei_printf("\nInference settings:\n");
  ei_printf("\tImage resolution: %dx%d\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
  
  if(fb){
    // Convert the captured RGB565 buffer to RGB888 buffer.
    ei_camera_capture_out = (uint8_t*)malloc(CAPTURED_IMAGE_BUFFER_COLS * CAPTURED_IMAGE_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if(!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_RGB565, ei_camera_capture_out)){ Serial.println("Camera => Cannot convert the RGB565 buffer to RGB888!"); return; }

    // Depending on the given model, resize the converted RGB888 buffer by utilizing built-in Edge Impulse functions.
    ei::image::processing::crop_and_interpolate_rgb888(
      ei_camera_capture_out, // Output image buffer, can be same as input buffer
      CAPTURED_IMAGE_BUFFER_COLS,
      CAPTURED_IMAGE_BUFFER_ROWS,
      ei_camera_capture_out,
      EI_CLASSIFIER_INPUT_WIDTH,
      EI_CLASSIFIER_INPUT_HEIGHT);

    // Run inference:
    ei::signal_t signal;
    // Create a signal object from the converted and resized image buffer.
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_cutout_get_data;
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

    // Obtain the object detection results and bounding boxes for the detected labels (classes). 
    bool bb_found = result.bounding_boxes[0].value > 0;
    for(size_t ix = 0; ix < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; ix++){
      auto bb = result.bounding_boxes[ix];
      if(bb.value == 0) continue;
      // Print the calculated bounding box measurements on the serial monitor.
      ei_printf("    %s (", bb.label);
      ei_printf_float(bb.value);
      ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n", bb.x, bb.y, bb.width, bb.height);
      // Get the imperative predicted label (class) and the detected object's bounding box measurements.
      if(bb.label == "red") predicted_class = 0;
      if(bb.label == "green") predicted_class = 1;
      if(bb.label == "blue") predicted_class = 2;
      box.x = String(bb.x);
      box.y = String(bb.y);
      box.w = String(bb.width);
      box.h = String(bb.height);
      ei_printf("\nPredicted Class: %d [%s]\n", predicted_class, classes[predicted_class]); 
    }
    if(!bb_found) ei_printf("\nNo objects found!\n");

    // Detect anomalies, if any:
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
      ei_printf("Anomaly: ");
      ei_printf_float(result.anomaly);
      ei_printf("\n");
    #endif 

    // Release the image buffer.
    free(ei_camera_capture_out);
  }
}

static int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr){
  // Convert the given image data (buffer) to the out_ptr format required by the Edge Impulse FOMO model.
  size_t pixel_ix = offset * 3;
  size_t pixels_left = length;
  size_t out_ptr_ix = 0;
  // Since the image data is converted to an RGB888 buffer, directly recalculate offset into pixel index.
  while(pixels_left != 0){  
    out_ptr[out_ptr_ix] = (ei_camera_capture_out[pixel_ix] << 16) + (ei_camera_capture_out[pixel_ix + 1] << 8) + ei_camera_capture_out[pixel_ix + 2];
    // Move to the next pixel.
    out_ptr_ix++;
    pixel_ix+=3;
    pixels_left--;
  }
  return 0;
}

void make_a_post_request(camera_fb_t * fb, String request){
  // Connect to the web application named mechanical_anomaly_detector. Change '80' with '443' if you are using SSL connection.
  if (client.connect(server, 80)){
    // If successful:
    Serial.println("\nConnected to the web application successfully!\n");
    // Create the query string:
    String query = application + request;
    // Make an HTTP POST request:
    String head = "--AnomalyResult\r\nContent-Disposition: form-data; name=\"resulting_image\"; filename=\"new_image.txt\"\r\nContent-Type: text/plain\r\n\r\n";
    String tail = "\r\n--AnomalyResult--\r\n";
    // Get the total message length.
    uint32_t totalLen = head.length() + fb->len + tail.length();
    // Start the request:
    client.println("POST " + query + " HTTP/1.1");
    client.println("Host: 192.168.1.22");
    client.println("Content-Length: " + String(totalLen));
    client.println("Connection: Keep-Alive");
    client.println("Content-Type: multipart/form-data; boundary=AnomalyResult");
    client.println();
    client.print(head);
    client.write(fb->buf, fb->len);
    client.print(tail);
    // Wait until transferring the image buffer.
    delay(2000);
    // If successful:
    Serial.println("HTTP POST => Data transfer completed!\n");
  }else{
    Serial.println("\nConnection failed to the web application!\n");
    delay(2000);
  }
}

bool save_image(fs::FS &fs, const char *file_name, uint8_t *data, size_t len){
  // Create a new file on the SD card.
  volatile boolean sd_run = false;
  File file = fs.open(file_name, FILE_WRITE);
  if(!file){ Serial.println("SD Card => Cannot create file!"); return sd_run; }
  // Save the given image buffer to the created file on the SD card.
  if(file.write(data, len) == len){
      Serial.printf("SD Card => IMG saved: %s\n", file_name);
      sd_run = true;
  }else{
      Serial.println("SD Card => Cannot save the given image!");
  }
  file.close();
  return sd_run;  
}

void screenDrawPixel(int16_t x, int16_t y, uint16_t color){
  // Draw a pixel (point) on the Fermion TFT LCD display.
  screen.writePixel(x,y,color);
}
