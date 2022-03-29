#include <Wire.h>

int pointer = 0;

void write_processing(int numBytes) {
    Serial.print(numBytes);
    Serial.println(" bytes written by controller -");
    for (int i=0; i<numBytes; i++) {
        int value = Wire.read();
        Serial.print(" ");
        Serial.print(value);
            // manage pointer
        if (i==0)
            pointer = value; 
        else
            pointer++;
    }
    Serial.println("");
}

void read_processing() {
    char response[5];
    Serial.print("Reading from ");
    Serial.println(pointer);
    for (int i=0; i<4; i++)
        response[i] = pointer++;
    response[4] = 0;
    Wire.write(response, 4);
}

void setup() {
  Serial.begin(9600);
  Wire.begin(8);        // address of target
  Wire.onReceive(write_processing);
  Wire.onRequest(read_processing);
}

void loop() {
        // add code to allow address to be changed
    delay(100);
}
