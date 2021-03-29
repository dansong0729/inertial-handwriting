#include <Arduino_LSM9DS1.h>

#define BUTT D2
#define DEBOUNCE_T 10l

enum state{COLLECT, WAIT, C2W_DEBOUNCE, W2C_DEBOUNCE} s = WAIT;
unsigned long pressed, started; //timestamps
float acc_avg_x=0;
float acc_avg_y=0;
float acc_avg_z=0;
float gyo_avg_x=0;
float gyo_avg_y=0;
float gyo_avg_z=0;
  
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
  pinMode(BUTT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTT), handle_butt, CHANGE);
  pressed = millis();
  if(!IMU.begin()){
    Serial.println("Failed to initialize IMU!");
    while(1);
  }
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
}

void loop() {
  switch(s){
    case C2W_DEBOUNCE:
      if(millis()-pressed>DEBOUNCE_T){
        s = WAIT;
        //write nans as that cannot be output by IMU
        int temp = 0x7FC00000;
        float nan = *(float*)&temp; //used to signal python code
        byte * val = (byte*) &nan;
        Serial.write(val,4);//1
        Serial.write(val,4);//2
        Serial.write(val,4);//3
        Serial.write(val,4);//4
        Serial.write(val,4);//5
        Serial.write(val,4);//6
        Serial.write(val,4);//7
        Serial.flush();
        return;
      }
    //intentional fall through
    case COLLECT:
      if(IMU.accelerationAvailable() && IMU.gyroscopeAvailable()){
        float x,y,z,rx,ry,rz;
        IMU.readAcceleration(x,y,z);
        IMU.readGyroscope(rx,ry,rz);
        x -= acc_avg_x;
        y -= acc_avg_y;
        z -= acc_avg_z;
        rx -= gyo_avg_x;
        ry -= gyo_avg_y;
        rz -= gyo_avg_z;
        unsigned long now = micros();
        float stamp = (float)(now-started);
        byte * val;
        val = (byte*) &x;
        Serial.write(val,4);
        val = (byte*) &y;
        Serial.write(val,4);
        val = (byte*) &z;
        Serial.write(val,4);
        val = (byte*) &rx;
        Serial.write(val,4);
        val = (byte*) &ry;
        Serial.write(val,4);
        val = (byte*) &rz;
        Serial.write(val,4);
        val = (byte*) &stamp;
        Serial.write(val,4);
      }
      break;
    case W2C_DEBOUNCE:
      if(millis()-pressed>DEBOUNCE_T){
        s = COLLECT;
        Serial.println("IMU goes BBBBRRRR");
        started = micros();
      }
      break;
    default:
      return;
  }
}
