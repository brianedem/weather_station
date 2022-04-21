#include <stdlib.h>
#include <ctype.h>
#include <Wire.h>
#include <EEPROM.h>

struct {
    uint8_t signature;
    uint8_t target_address;
    uint8_t response_length;
} ee_image;

int pointer = 0;
uint8_t target_address = 8;
unsigned response_length = 4;

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

#define MAX_RESPONSE_LENGTH  16
char response[MAX_RESPONSE_LENGTH];
void read_processing() {
    Serial.print("Reading from ");
    Serial.println(pointer);
    for (int i=0; i<response_length; i++)
        response[i] = pointer++;
    response[4] = 0;
    Wire.write(response, response_length);
}

void setup() {
  Serial.begin(9600);
  Wire.begin(target_address);
  Wire.onReceive(write_processing);
  Wire.onRequest(read_processing);
  Serial.begin(9600);
  delay(1000);
  EEPROM.get(0, ee_image);
  if (0) {
      Serial.println("ee_values: ");
      Serial.println(ee_image.signature);
      Serial.println(ee_image.target_address);
      Serial.println(ee_image.response_length);
  }
  if (ee_image.signature==0xA5) {
    target_address = ee_image.target_address;
    response_length = ee_image.response_length;
  }

  Serial.print("Target address is ");
  Serial.print(target_address);
  Serial.println(" (7-bit)");
  Serial.print("Response length is ");
  Serial.print(response_length);
  Serial.println(" bytes");
  Serial.print("Target >");
}

const char *help_message =
    "commands:\r\n"
    "  r // reset interface\r\n"
    "  a // target address (7-bit)\r\n"
    "  l // length of response, in bytes\r\n"
    "  s // save configuration changes to EEPROM\r\n";

const int BUFFER_SIZE = 100;
char buf[BUFFER_SIZE];
int buf_index = 0;

void loop() {
        // add code to allow address to be changed
  if (Serial.available() > 0) {
    char c[2];
    int rlen = Serial.readBytes(c, 1);
    if (c[0]!='\r') {
        Serial.print(c[0]);
        buf[buf_index++] = c[0];
        if (buf_index==BUFFER_SIZE) {
            Serial.println("command too long");
            buf_index = 0;
        }
    }
    else {
        Serial.println("");
        buf[buf_index] = '\0';
        buf_index = 0;
        char *cmd = buf;
        char *endptr;
        unsigned parameter;
        unsigned param_count;

        do {
                // strip leading whitespace from command
            while (isWhitespace(*cmd))
                cmd++;

                // check for a command
            if (*cmd=='\0')
                break;

            char command = tolower(*cmd++);

                // strip leading whitespace from parameter
            while (isWhitespace(*cmd))
                cmd++;

                // check for a parameter
            if (*cmd=='\0') {
                param_count = 0;
            }
            else {
                parameter = strtol(cmd, &endptr, 0);
                if (cmd==endptr || *endptr!='\0') {
                    Serial.println("parameter format error");
                    break;
                }
                param_count = 1;
            }

                // command parsing - possible commands:
                //  r - reset the I2C interface
            if (command=='r') {
                if (param_count!=0)
                    Serial.println("Ignoring parameter");
                Serial.println("Resetting hardware interface");
                Wire.end();
                Wire.begin(target_address);
            }

                //  a - I2C target address (7-bit)
            else if (command=='a') {
                if (param_count!=1) {
                    Serial.println("Address command is missing address parameter");
                    break;
                }
                target_address = parameter;
                Serial.print("Changing target address to ");
                Serial.println(target_address);
                Wire.end();
                Wire.begin(target_address);
            }

                //  l - maximum length of read response
            else if (command=='l') {
                if (param_count!=1) {
                    Serial.println("Length command is missing address parameter");
                    break;
                }
                if (parameter > MAX_RESPONSE_LENGTH) {
                    Serial.println("Length parameter is too large");
                    break;
                }
                response_length = parameter;
                Serial.print("Setting response length to ");
                Serial.println(response_length);
            }
                // s - save configuration
            else if (command=='s') {
                if (param_count!=0)
                    Serial.println("Ignoring parameter");
                ee_image.signature = 0xA5;
                ee_image.target_address = target_address;
                ee_image.response_length = response_length;
                EEPROM.put(0, ee_image);
            }
            else {
                Serial.print("Unknown command ");
                Serial.println(command);
                Serial.print(help_message);
                Serial.print("Target address is ");
                Serial.println(target_address);
                Serial.print("Response length is ");
                Serial.print(response_length);
                Serial.println(" bytes");
            }
                
        } while (0);
        Serial.print("Target >");
    }
  }
}
