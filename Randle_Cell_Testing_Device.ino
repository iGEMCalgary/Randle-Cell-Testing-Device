//
//2020 iGEM Calgary Electrochemical Impedance Spectroscopy
//
//This code is designed for the Pmod Ia board by Digilent. 
//Using the AD5933 Impedance Analyser Integrated Circuit
//Refer to AD5933 Datasheet for more information
//
//By Mackenzie Sampson and Randy Moore
//
//
//****Code is set to export data to the serial monitor in .CSV format*****
// Uncomment the Serial.print() lines for general troubleshooting
//
//
//

//Convert the hex codes into binary and use this table to determine the command sent to the control registry.
// "D" refers to the corresponding bit in the registry
//
//   Table 9. Control Register Map (D15 to D12)
//   D15 D14 D13 D12 Function
//   0    0   0   0  No operation
//   0    0   0   1  Initialize with start frequency
//   0    0   1   0  Start frequency sweep
//   0    0   1   1  Increment frequency
//   0    1   0   0  Repeat frequency
//   1    0   0   0  No operation
//   1    0   0   1  Measure temperature
//   1    0   1   0  Power-down mode
//   1    0   1   1  Standby mode
//   1    1   0   0  No operation
//   1    1   0   1  No operation

//   Table 10. Control Register Map (D10 to D9)
//   D10 D9 Range No. Output Voltage Range
//   0   0    1        2.0 V p-p typical
//   0   1    4        200 mV p-p typical
//   1   0    3        400 mV p-p typical
//   1   1    2        1.0 V p-p typical

//   Table 11. Control Register Map (D11, D8 to D0)
//   Bits Description
//   D11  No operation
//   D8   PGA gain; 0 = ×5, 1 = ×1
//   D7   Reserved; set to 0
//   D6   Reserved; set to 0
//   D5   Reserved; set to 0
//   D4   Reset
//   D3   External system clock; set to 1
//        Internal system clock; set to 0
//   D2   Reserved; set to 0
//   D1   Reserved; set to 0
//   D0   Reserved; set to 0

#include <Wire.h>

#define SLAVE_ADDRESS 0x0D //When the device is powered up, it has a default serial bus address, 0001101 (0x0D)
#define ADDRESS_POINTER 0xB0 //Address pointer

//Assign each constant to its corresponding registry address from the registry map.

#define CONTROL_REGISTRY 0x80 //D15 to D8 Control registry
//#define CONTROL_REGISTRY_R2 0x81 //D7 to D0

#define START_FREQUENCY_R1 0x82 //D23 to D16 Start Frequency 
#define START_FREQUENCY_R2 0x83 //D15 to D8
#define START_FREQUENCY_R3 0x84 //D7 to D0

#define FREQUENCY_INCREMENT_R1 0x85 //D23 to D16 Frequency increment 
#define FREQUENCY_INCREMENT_R2 0x86 //D15 to D8
#define FREQUENCY_INCREMENT_R3 0x87 //D7 to D0

#define NUMBER_INCREMENTS_R1 0x88 //D15 to D8 Number of increments 
#define NUMBER_INCREMENTS_R2 0x89 //D7 to D0

#define NUMBER_SETTLING_CYCLES_R1 0x8A //D15 to D8 Number of settling time cycles
#define NUMBER_SETTLING_CYCLES_R2 0x8B //D7 to D0

#define STATUS_REGISTRY 0x8F //D7 to D0 Status registry D1 is a real number, D3 is imaginary. Will return 8 bits.

#define TEMPERATURE_R1 0x92 //D15 to D8 Temperature Data
#define TEMPERATURE_R2 0x93 //D7 to D0

#define REAL_DATA_R1 0x94 //D15 to D8 Real Data
#define REAL_DATA_R2 0x95//D7 to D0

#define IMAGINARY_DATA_R1 0x96 //D15 to D8 Imaginary Data
#define IMAGINARY_DATA_R2 0x97 //D7 to D0

//the internal gain setting resistor at pin 2 *Specific to the Pmodl Ia board*
int internal_resistor = 2;

//set up initial constants
const float MCLK = 16.776 * pow(10, 6); // AD5933 Internal Clock Speed @ 16.776 MHz

const int start_frequency = 1950; //Start Frequency @ 1950 Hz
const int increase_frequency = 975; //Increase in frequency @ 975 Hz
const int number_increments = 400; //Number of increments of frequency - max 511

char state;

//When measuring outside of ZMIN, 4.9 kΩ and ZMAX, 47.2 kΩ, these values need to change. **See table 1 on datasheet to determine range**
//const int PGA_gain = 1; //PGA gain allows the user to amplify the response signal into the ADC by a multiplication factor of ×5 or ×1. Automatically at 1
//const int v_out = 2; //Output voltage range set at 2V p-p
//const int s_time = 1; //settling time is 1 ms worst case


void setup() {

  Wire.begin();//Start i2c signal
  Serial.begin(9600);

  pinMode(internal_resistor, INPUT);
  digitalWrite(internal_resistor, HIGH);//high selects 20 Ohm gain setting resistor and low selects 100k Ohm gain setting resistor

  program_registry(); //The first step is to program the registry

}

void loop() {
  delay(5000);

  measureTemperature();

  if (Serial.available() > 0) {
    state = Serial.read();
    switch (state) {

      case 'T': //type T in the serial monitor to measure temperature
        measureTemperature();
        break;

      case 'R': //type R in the serial monitor to run sweep
        run_sweep();
        delay(1000);
        break;
    }
  }
}

//Write each command to its corresponding registry? I think. make sure to check that the
void program_registry() {

  //reset the part. Sets it to range 1 at ZMIN, 4.9 kΩ and ZMAX, 47.2 kΩ with a v_out at 1.98
  writeData(CONTROL_REGISTRY, 0x01);

  //program the start frequency
  writeData(START_FREQUENCY_R1, getFrequency(start_frequency, 1));
  writeData(START_FREQUENCY_R2, getFrequency(start_frequency, 2));
  writeData(START_FREQUENCY_R3, getFrequency(start_frequency, 3));

  //program the change in frequency
  writeData(FREQUENCY_INCREMENT_R1, getFrequency(increase_frequency, 1));
  writeData(FREQUENCY_INCREMENT_R2, getFrequency(increase_frequency, 2));
  writeData(FREQUENCY_INCREMENT_R3, getFrequency(increase_frequency, 3));

  //program the number of incriments (100), max 511
  writeData(NUMBER_INCREMENTS_R1, (number_increments & 0x001F00) >> 0x08 );
  writeData(NUMBER_INCREMENTS_R2, (number_increments & 0x0000FF));

  //program the delay in the measurements. The worst case is at maximum frequency.
  writeData(NUMBER_SETTLING_CYCLES_R1, 0x00);// Also see 0x07
  writeData(NUMBER_SETTLING_CYCLES_R1, 0x0C);// Also see 0xFF

  //initialize the system
  writeData(CONTROL_REGISTRY, 0x11);

  delay(100);
}

//Sweep a number of frequencies and calculate the impedance for each
void run_sweep() {
  short real;
  short imaginary;
  double frequency;
  double magnitude;
  double phase;
  double gain;
  double impedance;
  int i=0;

  //put the board into Standby by sending the "D" command '10110000' into the control registry using hex
  writeData(CONTROL_REGISTRY, 0xB0);

  //reset the board
  writeData(CONTROL_REGISTRY, 0x10);

  //Initialize the sweep with the start frequency
  writeData(CONTROL_REGISTRY, 0x20);

  Serial.println("Imaginary,Real,Frequency,Impedance,Magnitude");

  while ((readData(STATUS_REGISTRY) & 0x07) < 8 ) { // Check that status reg != 4, sweep not complete, STATUS_REGISTRY = 0x8F "When status reg = 4, it means you have valid real and imaginary data". Setting to <8 stops it once sweep is done
    delay(300); // delay between measurements

    int flag = readData(STATUS_REGISTRY) & 2;

    //Serial.println("");
    //Serial.println(readData(STATUS_REGISTRY));

    //checks to see if the status registry has values
    if (flag == 2) {

      byte R1 = readData(REAL_DATA_R1); //gets real data
      byte R2 = readData(REAL_DATA_R2);
      real = (R1 << 8) | R2;

      R1  = readData(IMAGINARY_DATA_R1);//gets imaginary data
      R2  = readData(IMAGINARY_DATA_R2);
      imaginary = (R1 << 8) | R2;

      // Serial.print("imaginary number");
      Serial.print(imaginary);
      Serial.print(",");

      // Serial.print("real number =");
      Serial.print(real);
      Serial.print(",");


      frequency = start_frequency + i * increase_frequency;
      magnitude = sqrt(pow(double(real), 2) + pow(double(imaginary), 2));

      phase = atan(double(imaginary) / double(real));
      phase = (180.0 / 3.1415926) * phase; //convert phase angle to degrees
      
      gain = 9.651421539 * pow(10, -7); //the gain must be calculated by (1/impedance)/magnitude of a known resistance to calibrate
      impedance = 1 / (gain * magnitude);

      //Serial.print("Frequency: ");
      Serial.print(frequency / 1000);
      //Serial.print(",kHz;");
      Serial.print(",");

      //Serial.print(" Impedance: ");
      Serial.print(impedance / 1000);
      //  Serial.print(",kOhm;");
      Serial.print(",");

      //Serial.print(" Magnitude: ");
      Serial.println(magnitude);
      //Serial.println(";");


      // Serial.print(" Phase: ");
      // Serial.print(phase);
      // Serial.println(";");

      if (i == number_increments) {

        break;
      }

      //Increase frequency
      if ((readData(STATUS_REGISTRY) & 0x07) < 4 ) {
        writeData(CONTROL_REGISTRY, 0x30);
        i++;
      }
    }
  }

  //power down '10100000'
  writeData(CONTROL_REGISTRY, 0xA0);
}



//Function responsible for programing the registry through the I2C protocol
void writeData(int address, int data) {
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(address);
  Wire.write(data);
  Wire.endTransmission();
  delay(10);
}


//Read the data that is sitting in the registries using I2C protocol
int readData(int address) {
  int data;

  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(ADDRESS_POINTER);
  Wire.write(address);
  Wire.endTransmission();

  delay(1);

  Wire.requestFrom(SLAVE_ADDRESS, 1);

  if (Wire.available() >= 1) {
    data = Wire.read();
  }
  else {
    data = -1;
  }

  delay(1);
  return data;
}


boolean measureTemperature() {
  // Measure temperature '10010000'
  writeData(CONTROL_REGISTRY, 0x90);

  delay(10);

  //Check status reg to see if temperature measurement is available
  int flag = readData(STATUS_REGISTRY) & 1;

  if (flag == 1) {

    // Temperature is available
    int temperatureData = readData(TEMPERATURE_R1) << 8;
    temperatureData |= readData(TEMPERATURE_R2);
    temperatureData &= 0x3FFF; // remove first two bits

    if (temperatureData & 0x2000 == 1) { // negative temperature
      temperatureData -= 0x4000;
    }

    double val = double(temperatureData) / 32;
    temperatureData /= 32;

    //Serial.print("Temperature: ");
    //Serial.print(val);
    //Serial.println("C.");

    // Power Down '10100000'
    writeData(CONTROL_REGISTRY, 0xA0);

    return true;
  }

  else {
    return false;
  }
}

//This will send commands to the control register using multiple bytes of space
byte getFrequency(float frequency, int n) {
  long val = long((frequency / (MCLK / 4)) * pow(2, 27));
  byte code;

  switch (n) {
    case 1:
      code = (val & 0xFF0000) >> 0x10;
      break;

    case 2:
      code = (val & 0x00FF00) >> 0x08;
      break;

    case 3:
      code = (val & 0x0000FF);
      break;

    default:
      code = 0;
  }

  return code;
}
