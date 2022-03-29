#include <stdlib.h>
#include <ctype.h>
#include <Wire.h>
const int BUFFER_SIZE = 100;
char buf[BUFFER_SIZE];
int buf_index = 0;

#define MAX_PARAMS 100
unsigned values[MAX_PARAMS];

void setup() {
  Wire.begin();
  Serial.begin(9600);
}
// command format:
//   i2c_cmd [; i2c_cmd]
// i2c_cmd:
//   i2c_read | i2c_write
// i2c_read:
//   r <address> <length>
// i2c_write:
//   w <address> <data> [<data>]*
int get_params(char *params) {
    
    int i = 0;
    char *eptr;
    while (1) {
        while (isWhitespace(*params))
            params++;
        if (*params=='\0')
            return i;
        values[i] = strtol(params, &eptr, 0);
        if (params==eptr) {
            Serial.print("unexpected parameter ");
            Serial.println(params);
            return 0;
        }
        i++;
        params = eptr;
    }
    return i;
}

void loop() {
  if (Serial.available() > 0) {
    char c[2];
    int rlen = Serial.readBytes(c, 1);
    if (c[0]!='\r') {
        Serial.print(c[0]);
        buf[buf_index++] = c[0];
        if (buf_index==BUFFER_SIZE) {
            Serial.println("command too long");
            buf_index = 0;
            return;
        }
    }
    else {
        Serial.println("");
        buf[buf_index] = '\0';
        buf_index = 0;
        char *cmd = buf;
        char *next_cmd;
        int stop;
        do {
            // split up command if there are multiple commands
          next_cmd = strchr(cmd, ';');
          if (next_cmd==NULL) {
            stop = 1;
          }
          else {
            *next_cmd++ = '\0';
            stop = 0;
          }

            // strip off leading whitespace from command
          while (isWhitespace(*cmd))
            cmd++;

            // read command processing
          if (tolower(*cmd)=='r') {
            int pcnt = get_params(cmd+1);
            if (pcnt!=1) {
                Serial.print("read command only supports one parameter - address\n");
                break;
            }
            Serial.print("read ");
            Serial.println(values[0]);

                // perform I2C read
            int count = Wire.requestFrom(8, 4);
            Serial.print(count);
            Serial.print(" bytes were returned:");
            while (count--) {
                unsigned c = Wire.read();
                Serial.print(" ");
                Serial.print(c);
            }
            Serial.println("");
          }

            // write command processing
          else if(tolower(*cmd)=='w') {
            int pcnt = get_params(cmd+1);
            if (pcnt<2) {
                Serial.print("write command requires more than one parameter\n");
                break;
            }
            Serial.print("write");
            for (int i=0; i<pcnt; i++) {
                Serial.print(" ");
                Serial.print(values[i]);
            }
            Serial.println("");

                // perform I2C write
            Wire.beginTransmission((char)values[0]);
            for (int i=1; i<pcnt; i++)
                Wire.write(values[i]);
            int tx_status = Wire.endTransmission(stop);
            switch (tx_status) {
            case 0:
                Serial.println("Transmission successful");
                break;
            case 1:
                Serial.println("Data exceeds 32-byte buffer");
                break;
            case 2:
                Serial.println("NACK on target address");
                break;
            case 3:
                Serial.println("NACK on transmit data");
                break;
            case 4:
                Serial.println("Unknown I2C error");
                break;
            }
          }
          else {
            Serial.print("Unknown command ");
            Serial.println(cmd);
            break;
          }
          cmd = next_cmd;
        } while (cmd!=NULL);
      }
  }
}
