#include <stdlib.h>
#include <ctype.h>
#include <Wire.h>
const int BUFFER_SIZE = 100;
char buf[BUFFER_SIZE];
int buf_index = 0;

#define MAX_PARAMS 25
unsigned values[MAX_PARAMS];

unsigned frequency = 100;           // default to 100 kHz
unsigned timeout = 100;     // default to 100 ms

void setup() {
  Wire.begin();
  Wire.setWireTimeout(timeout*1000L, true);
  Serial.begin(9600);
  Serial.print("Controller >");
}
// command format:
//   i2c_cmd [; i2c_cmd]
// i2c_cmd:
//   i2c_read | i2c_write
// i2c_read:
//   r <address> <length>
// i2c_write:
//   w <address> <data> [<data>]*
// bus timeout:
//   t <value_in_ms>
// frequency:
//   f <value_in_kHz>
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
const char *help_message = 
    "commands:\r\n"
    "  r address [length] // reads length bytes from device at address\r\n"
    "  w address data*  // writes to device at address one or more bytes\r\n"
    "  t value          // enable bus timeout reset, value in ms (zero disables)\r\n"
    "  f value          // set I2C bus clock frequency in kHz\r\n"
    "Addresses are 7-bit\r\n"
    "To specify a sequence of commands separated with RESTART enter the\r\n"
    "commands on a single line separated with \";\"\r\n";

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
            int length;
            if (pcnt==1)
                length = 4;
            else if (pcnt==2)
                length = values[1];
            else {
                Serial.println("read command one or two parameters - address [length]");
                break;
            }
            Serial.print("reading ");
            Serial.print(length);
            Serial.print(" bytes from ");
            Serial.println(values[0]);

                // perform I2C read
            int count = Wire.requestFrom(values[0], length);
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
                Serial.println("write command requires more than one parameter");
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

            // bus timeout control
          else if (tolower(*cmd)=='t') {
            int pcnt = get_params(cmd+1);
            if (pcnt!=1) {
                Serial.println("timeout command requires a single parameter");
                break;
            }
            timeout = values[0];
            Wire.setWireTimeout(timeout*1000L, true);
            if (values[0]==0)
                Serial.println("timeout disabled");
            else {
                Serial.print("timeout set to ");
                Serial.print(timeout);
                Serial.println(" milliseconds");
            }
          }

            // bus frequency
          else if (tolower(*cmd)=='f') {
            int pcnt = get_params(cmd+1);
            if (pcnt!=1) {
                Serial.println("speed command requires a single parameter");
                break;
            }
            if (values[0] < 10 || values[0]> 1000) {
                Serial.println("frequency value out of range");
            }
            else {
                frequency = values[0];
                Wire.setClock(frequency*1000L);
                Serial.print("frequency set to ");
                Serial.print(frequency);
                Serial.println(" kHz");
            }
          }

          else {
            if (*cmd) {
                Serial.print("Unknown command ");
                Serial.println(cmd);
                Serial.print(help_message);
                Serial.print(" Frequency set to ");
                Serial.print(frequency);
                Serial.println(" kHz");
                Serial.print(" Timeout is ");
                Serial.print(timeout);
                Serial.println(" ms");
            }
            break;
          }
          cmd = next_cmd;
        } while (cmd!=NULL);
        Serial.print("Controller >");
      }
  }
}
