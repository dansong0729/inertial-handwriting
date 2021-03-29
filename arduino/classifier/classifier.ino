#include <Arduino_LSM9DS1.h>

//Tensorflow imports
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include "model.h"

//BUTT stuff
#define BUTT D2
#define DEBOUNCE_T 10l

//data
#define NUM_GESTURES 10
#define FEAT_ROW 7 //features per read
#define MAX_SAMPLES 3*119

//normalization params
#define X_MEAN -4.99124042e-02
#define Y_MEAN 1.47621968e-02
#define Z_MEAN -1.83530803e-03
#define RX_MEAN -3.67210127e-01
#define RY_MEAN -4.91922590e+00
#define RZ_MEAN 2.36126592e-01
#define X_STD 2.35764548e-01
#define Y_STD 3.50199012e-01
#define Z_STD 4.15753254e-01
#define RX_STD 1.85064333e+01
#define RY_STD 5.26089374e+01
#define RZ_STD 5.26107051e+01
#define TIME_STD 4.37490816e+05

//data collection related globals
int samplesRead = 0;
enum state{COLLECT, WAIT, C2W_DEBOUNCE, W2C_DEBOUNCE} s = WAIT;
unsigned long pressed, started; //timestamps
float acc_avg_x=0;
float acc_avg_y=0;
float acc_avg_z=0;
float gyo_avg_x=0;
float gyo_avg_y=0;
float gyo_avg_z=0;

// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;
tflite::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

constexpr int tensorArenaSize = 128 * 1024;//8*1024
byte tensorArena[tensorArenaSize];

//annoying stuff
const char* GESTURES[] = {
  "0",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9",
};

void handle_butt(){ //>:)
  unsigned long now = millis();
  if (digitalRead(BUTT)==HIGH){ //released
    s = C2W_DEBOUNCE;
  } else{ //pressed
    s = W2C_DEBOUNCE;
  }
  pressed = now;
}

void setup() {
  Serial.begin(38400);
  //BUTTon stuff
  pinMode(BUTT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTT), handle_butt, CHANGE);
  pressed = millis();
  
  //IMU init
  if(!IMU.begin()){
    while(1);
  }
  //get offsets (takes about a second)
  float x,y,z,rx,ry,rz;
  for (int i=0;i<100;i++) {
    while (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()){
    }
    IMU.readAcceleration(x,y,z);
    IMU.readGyroscope(rx,ry,rz);
    acc_avg_x += x;
    acc_avg_y += y;
    acc_avg_z += z;
    gyo_avg_x += rx;
    gyo_avg_y += ry;
    gyo_avg_z += rz;
  }
  acc_avg_x /= 100;
  acc_avg_y /= 100;
  acc_avg_z /= 100;
  gyo_avg_x /= 100;
  gyo_avg_y /= 100;
  gyo_avg_z /= 100;

  tflModel = tflite::GetModel(model);
  
  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }
  
  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);
  
  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();
  
  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
  
  TfLiteStatus invokeStatus = tflInterpreter->ResetVariableTensors();
  if (invokeStatus != kTfLiteOk) {
    Serial.println("Model reset failed!");
    while (1);
    return;
  }
}

void loop() {
  switch(s){
    case C2W_DEBOUNCE:
      if(millis()-pressed>DEBOUNCE_T){
        s = WAIT;
        //Inference
        TfLiteStatus invokeStatus = tflInterpreter->Invoke();
        if (invokeStatus != kTfLiteOk) {
          Serial.println("Invoke failed!");
          while (1);
          return;
        }

        //sort outputs
        float scores[NUM_GESTURES];
        int guesses[NUM_GESTURES];
        float score;
        for (int i = 0; i < NUM_GESTURES; i++) {
          score = tflOutputTensor->data.f[i];
          int j = i;
          for (; j>0 && score > scores[j-1]; j--){
            scores[j] = scores[j-1];
            guesses[j] = guesses[j-1];
          }
          scores[j] = score;
          guesses[j] = i;
        }
        //print outputs
        for (int i = 0; i < NUM_GESTURES; i++){
          if (scores[i] < 0.1){
            break; 
          }
          Serial.print(guesses[i]);
          Serial.print(" with confidence: ");
          Serial.println(scores[i], 4);
        }
        Serial.println();

        //reset everything
        invokeStatus = tflInterpreter->ResetVariableTensors();
        if (invokeStatus != kTfLiteOk) {
          Serial.println("Model reset failed!");
          while (1);
          return;
        }
        for (int i=0; i < MAX_SAMPLES; i++){
          tflInputTensor->data.f[i * FEAT_ROW + 0] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 1] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 2] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 3] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 4] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 5] = 0;
          tflInputTensor->data.f[i * FEAT_ROW + 6] = 0;
        }
        return;
      }
    //intentional fall through
    case COLLECT:
      if(IMU.accelerationAvailable() && IMU.gyroscopeAvailable()
          && samplesRead<MAX_SAMPLES){
        //read data
        float x,y,z,rx,ry,rz;
        IMU.readAcceleration(x,y,z);
        IMU.readGyroscope(rx,ry,rz);
        //normalize
        x -= acc_avg_x + X_MEAN;
        y -= acc_avg_y + Y_MEAN;
        z -= acc_avg_z + Z_MEAN;
        rx -= gyo_avg_x + RX_MEAN;
        ry -= gyo_avg_y + RY_MEAN;
        rz -= gyo_avg_z + RZ_MEAN;
        x /= X_STD;
        y /= Y_STD;
        z /= Z_STD;
        rx /= RX_STD;
        ry /= RY_STD;
        rz /= RZ_STD;

        //get timestamp
        unsigned long now = micros();
        float stamp = (float)(now-started);
        stamp /= TIME_STD;
        //write values
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 0] = x;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 1] = y;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 2] = z;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 3] = rx;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 4] = ry;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 5] = rz;
        tflInputTensor->data.f[samplesRead * FEAT_ROW + 6] = stamp;
        
        samplesRead++;
      }
      break; //end COLLECT case
    case W2C_DEBOUNCE:
      if(millis()-pressed>DEBOUNCE_T){
        s = COLLECT;
        samplesRead = 0;
        started = micros();
      }
      break;
    default:
      return;
  }
}
