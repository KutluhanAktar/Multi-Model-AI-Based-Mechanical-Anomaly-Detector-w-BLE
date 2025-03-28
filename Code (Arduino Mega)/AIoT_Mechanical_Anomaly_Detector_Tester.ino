         /////////////////////////////////////////////  
        //   Multi-Model AI-Based Mechanical       //
       //        Anomaly Detector w/ BLE          //
      //             ---------------             //
     //             (Arduino Mega)              //           
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
// Arduino Mega :
//                                Short-Shaft Linear Potentiometer (R)
// A0      ------------------------ S
//                                Short-Shaft Linear Potentiometer (L)
// A1      ------------------------ S
//                                SG90 Mini Servo Motor (R)
// D2      ------------------------ Signal
//                                SG90 Mini Servo Motor (L)
// D3      ------------------------ Signal


// Include the required libraries:
#include <Servo.h>

// Define servo motors.
Servo right;
Servo left;

// Define control potentiometers.
#define pot_right A0
#define pot_left A1

// Define the data holders.
int turn_right = 0, turn_left = 0;

void setup(){
  Serial.begin(9600);

  // Initiate servo motors.
  right.attach(2); 
  delay(500);
  left.attach(3);
  delay(500);
}

void loop(){
  // Depending on the potentiometer positions, turn servo motors (0 - 180).
  turn_right = map(analogRead(pot_right), 0, 1023, 0, 180);
  turn_left = map(analogRead(pot_left), 0, 1023, 0, 180);
  right.write(turn_right);                  
  delay(15);                          
  left.write(turn_left);
  delay(15);
}
