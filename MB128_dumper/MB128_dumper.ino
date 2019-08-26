
#include <SD.h>

//
// This sketch is intended to be a MB128 host, attempting
// to extract the contents of the MB128 memory
//

// ------------------------
// *** HARDWARE defines ***
// ------------------------

// This sketch is made specifically for the Adafruit M0 Feather Adalogger
// We are using the pins marked as D09 and D10 for input
// These are the internal mappings for those external pin identifiers
// so that we can access them directly (...and 10 times faster !)
#define IN_PORT            REG_PORT_IN0
#define OUT_PORT_SET       REG_PORT_OUTSET0
#define OUT_PORT_CLR       REG_PORT_OUTCLR0

// inputs
#define MB128_D3_INPIN     PORT_PA16    // Pin marked as D11 on M0 Feather Adalogger (D3 on PCE joypad)
#define MB128_D2_IDENTPIN  PORT_PA18    // Pin marked as D10 on M0 Feather Adalogger (D2 on PCE joypad)
#define MB128_D1_INPIN     PORT_PA20    // Pin marked as D6  on M0 Feather Adalogger (D1 on PCE joypad)
#define MB128_D0_DATAINPIN PORT_PA15    // Pin marked as D5  on M0 Feather Adalogger (D0 on PCE joypad)

// outputs
#define MB128_DATAOUTPIN   PORT_PA19    // Pin marked as D12 on M0 Feather Adalogger (SEL on PCE joypad)
#define MB128_CLOCKPIN     PORT_PA17    // Pin marked as D13 on M0 Feather Adalogger (CLR on PCE joypad)


const bool delayMilli = false;  // if true, delay in milliseconds
const int delayShort = 2;  // 2uS on real system
const int delayLong = 4;   // 4uS on real system



#if defined(ADAFRUIT_FEATHER_M0)
//const int clockPin = 12;   // clock to MB128
//const int dataoutPin = 11; // data going to MB128
const int clockPin = 13;     // clock to MB128 (CLR)
const int dataoutPin = 12;   // data going to MB128 (SEL)

const int d3_inPin = 11;      // data 3 pin from MB128
const int d2_identPin = 10;   // data 2 - identification pin from MB128
const int d1_inPin = 6;       // data 1 pin from MB128
const int d0_datainPin = 5;   // data 0 - data pin from MB128 (d0)

const int chipSelect = 4;
#endif



char inChar;
boolean cardPresent;
boolean dataFilePresent;

unsigned long time_s;
unsigned long time_e;

File dataFile;
File logFile;

uint8_t logBuf[16];

char to_hex(char in_nybble)
{
char out_char;
  in_nybble = in_nybble & 0x0f;
  
  if (in_nybble < 10)
    out_char = '0' + in_nybble;
  else
    out_char = 'A' - 10 + in_nybble;
    
  return(out_char);
}


void delay_short()
{
  if (delayShort == 0)
    return;

  if (delayMilli)
    delay(delayShort);
  else
    delayMicroseconds(delayShort);
}

void delay_long()
{
  if (delayLong == 0)
    return;

  if (delayMilli)
    delay(delayLong);
  else
    delayMicroseconds(delayLong);
}

void log_access(char sendrec, bool clk, bool sel)
{
  uint32_t  inport;
  
  //** READ the return ports
  inport = IN_PORT;

  logBuf[0] = sendrec;
  logBuf[1] = ' ';
  logBuf[2] = clk ? '1' : '0';
  logBuf[3] = sel ? '1' : '0';


  logBuf[4] = ' ';      // space

  logBuf[5] = (inport & MB128_D3_INPIN) ? '1' : '0';
  logBuf[6] = (inport & MB128_D2_IDENTPIN) ? '1' : '0';
  logBuf[7] = (inport & MB128_D1_INPIN) ? '1' : '0';
  logBuf[8] = (inport & MB128_D0_DATAINPIN) ? '1' : '0';

  logBuf[9] = 0x0d;
  logBuf[10] = 0x0a;
  logFile.write(&logBuf[0], 10);
}


void mb128_send_bit(bool outbit)
{
// ***
//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;
  
//  digitalWrite(dataoutPin, outbit);
  if (outbit)
    OUT_PORT_SET = MB128_DATAOUTPIN;
  else
    OUT_PORT_CLR = MB128_DATAOUTPIN;


  delay_short();
  
//  digitalWrite(clockPin, HIGH);
  OUT_PORT_SET = MB128_CLOCKPIN;

  delay_long();

// log access
  log_access('S', true, outbit);

//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;

  delay_short();

// log access
  log_access('S', false, outbit);
}

void mb128_send_byte(char outbyte)
{
bool outbit;
char i = 8;

  logFile.print("Send Byte 0x");
  logFile.print(((outbyte >> 4)& 0x0f), HEX);
  logFile.println((outbyte & 0x0f), HEX);

  while (i > 0) {
    if (outbyte & 0x01)
      outbit = true;
    else
      outbit = false;

// ***
//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;

//    digitalWrite(dataoutPin, outbit);
    if (outbit)
      OUT_PORT_SET = MB128_DATAOUTPIN;
    else
      OUT_PORT_CLR = MB128_DATAOUTPIN;

    delay_short();

//    digitalWrite(clockPin, HIGH);
    OUT_PORT_SET = MB128_CLOCKPIN;

    delay_long();
    
// log access
    log_access('S', true, outbit);

//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;
  
    delay_short();

// log access
    log_access('S', false, outbit);

    outbyte = outbyte >> 1;
    i--;
  }
  logFile.println();

}

boolean mb128_read_bit()
{
bool inbit;
uint32_t inport;

// ***
//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;
//  digitalWrite(dataoutPin, LOW);
  OUT_PORT_CLR = MB128_DATAOUTPIN;
  delay_short();

// ***
//  digitalWrite(clockPin, HIGH);
  OUT_PORT_SET = MB128_CLOCKPIN;
  delay_short();

// log access
  log_access('R', true, false);

// ***  
//  inbit = digitalRead(d0_datainPin);
  inbit = (IN_PORT & MB128_D0_DATAINPIN);

//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;
  delay_short();

// log access
  log_access('R', false, false);

  return(inbit);
}

char mb128_read_byte()
{
bool inbit;
char inbyte = 0;
char i = 8;

  logFile.println("Read Byte");

  while (i > 0) {
//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;
//    digitalWrite(dataoutPin, LOW);
    OUT_PORT_CLR = MB128_DATAOUTPIN;
    delay_short();
// ***
//    digitalWrite(clockPin, HIGH);
    OUT_PORT_SET = MB128_CLOCKPIN;
    delay_short();

// log access
    log_access('R', true, false);

// ***  
//  inbit = digitalRead(d0_datainPin);
    inbit = (IN_PORT & MB128_D0_DATAINPIN);

//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;
    delay_short();

// log access
    log_access('R', false, false);

    inbyte = inbyte >> 1;

    if (inbit == true)
      inbyte = inbyte | 0x80;
    else
      inbyte = inbyte & 0x7f;
    
    i--;
  }
  logFile.print("Byte Read = 0x");
  logFile.print(((inbyte >> 4)& 0x0f), HEX);
  logFile.println((inbyte & 0x0f), HEX);
  logFile.println();

  return(inbyte);
}

void joyport_init()
{
  // simulate a joyport scan across 5 joypads
  //
  logFile.println("joyport init");
  mb128_send_bit(true);
  logFile.println("");
  
  delay_long();
  digitalWrite(dataoutPin, LOW);
  delay_short();
  digitalWrite(dataoutPin, HIGH);
  delay_short();
  digitalWrite(dataoutPin, LOW);
  delay_short();
  digitalWrite(dataoutPin, HIGH);
  delay_short();
  digitalWrite(dataoutPin, LOW);
  delay_short();
  digitalWrite(dataoutPin, HIGH);
  delay_short();
  digitalWrite(dataoutPin, LOW);
  delay_short();
  digitalWrite(dataoutPin, HIGH);
  delay_short();
  digitalWrite(dataoutPin, LOW);
  delay_short();
  digitalWrite(dataoutPin, HIGH);
  delay(10);
}

bool mb128_detect()
{
char i = 4;   // number of retries
char joy_out = 0;

  logFile.println("Detect - write 0xa8");

  while (i > 0) {
    mb128_send_byte(0xa8);

  logFile.println("Detect - read bit #1");
    mb128_send_bit(false);
    
// ***
//    if (digitalRead(d2_identPin))
    if (IN_PORT & MB128_D2_IDENTPIN)
      joy_out = joy_out | 0x40;

// ***
//    if (digitalRead(d0_datainPin))
    if (IN_PORT & MB128_D0_DATAINPIN)
      joy_out = joy_out | 0x10;
  
  logFile.println("Detect - read bit #2");
    mb128_send_bit(true);

// ***
//    if (digitalRead(d2_identPin))
    if (IN_PORT & MB128_D2_IDENTPIN)
      joy_out = joy_out | 0x4;

// ***
//    if (digitalRead(d0_datainPin))
    if (IN_PORT & MB128_D0_DATAINPIN)
      joy_out = joy_out | 0x1;
  
    if (joy_out == 0x04) {
      logFile.println("");
      return(true);
    }
    else {
      Serial.print("joy_out = ");
      Serial.println(joy_out, HEX);
    }
  
    joy_out = 0;
    i = i -1;
  }

  // send three zero bits if it was not detected
  logFile.println("Detect - send 3 trailing bits");
  mb128_send_bit(false);
  mb128_send_bit(false);
  mb128_send_bit(false);
  logFile.println("");
  
  return(false);
}

bool mb128_boot()
{
char i = 8;  // number of retries
bool found = false;
bool temp;

  while (i > 0) {
    found = mb128_detect();
    if (found)
      break;
    i--;
  }
  if ((i == 0) && (!found))
    return(false);

  logFile.println("Boot lead");
  mb128_send_bit(true);   // 1
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  
  logFile.println("Boot Byte 0x00");
  mb128_send_byte(0x00);  // 0x00
  
  logFile.println("Boot Byte 0x01");
  mb128_send_byte(0x01);  // 0x01
  
  logFile.println("Boot Byte 0x00");
  mb128_send_byte(0x00);  // 0x00
  
  logFile.println("Boot 4 bits");
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  
  logFile.println("Boot readbit");
  temp = mb128_read_bit();
  Serial.print("boot readbit = ");
  if (temp)
    Serial.println("1");
  else
    Serial.println("0");
  
  logFile.println("Boot trail 3 bits");
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  logFile.println("");

  return(true);
}

#define MB128_READ_SECTOR   true
#define MB128_WRITE_SECTOR  false

void mb128_rdwr_sector_num(bool rdwr, char sector_num)
{
  logFile.println("Read-write header");
  mb128_send_bit(rdwr);   // 1 = read; 0 = write

  logFile.println("Read-write address");
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_byte(sector_num);  // sector number

  logFile.println("Read-write length");
  mb128_send_byte(0x00);  // 0x00
  mb128_send_byte(0x10);  // 0x10
  
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0  
  logFile.println("");
}

bool mb128_read_sectors(char start_sector, int num_sectors)
{
int i, j;
char read_byte;
char disp_char;
char curr_sector = start_sector;
char char_buf[16];

  while (num_sectors > 0) {
    if (!mb128_detect())
      return(false);

    logFile.print("Read sector 0x");
    logFile.print(((curr_sector >> 4)& 0x0f), HEX);
    logFile.println((curr_sector & 0x0f), HEX);

    mb128_rdwr_sector_num(MB128_READ_SECTOR, curr_sector);
    Serial.print("Sector #");
    Serial.print(((curr_sector >> 4) & 0x0f), HEX);
    Serial.println((curr_sector & 0x0f), HEX);

    for (i = 0; i < 512; i+=16 ) {
//      Serial.print(((i >> 8) & 0x0f), HEX);
//      Serial.print(((i >> 4) & 0x0f), HEX);
//      Serial.print((i & 0x0f), HEX);
//      Serial.print(": ");
      for (j = 0; j < 16; j++) {
        read_byte = mb128_read_byte();
        if (cardPresent)
          dataFile.write(read_byte);

        if ((read_byte < 0x20) || (read_byte > 0x7f))
          disp_char = '.';
        else
          disp_char = read_byte;
          
        char_buf[j] = disp_char;

//        Serial.print(((read_byte >> 4) & 0x0f), HEX);
//        Serial.print((read_byte & 0x0f), HEX);
//        Serial.print(" ");
      }
      for (j = 0; j < 16; j++) {
//        Serial.print(char_buf[j]);        
      }
//      Serial.println("");
    }
//    Serial.println("");
    curr_sector++;
    num_sectors--;

    logFile.println("Sector read trailing bits");
    mb128_send_bit(false);  // 0
    mb128_send_bit(false);  // 0
    mb128_send_bit(false);  // 0
    logFile.println("");
  }

  return(true);
}

bool mb128_write_sectors(char start_sector, int num_sectors)
{
int i, j;
char read_byte;
char disp_char;
char curr_sector = start_sector;
char char_buf[16];
bool temp;


  if (!dataFilePresent) {
    return(false);
  }

  while (num_sectors > 0) {
    if (!mb128_detect())
      return(false);

    logFile.print("Write sector 0x");
    logFile.print(((curr_sector >> 4)& 0x0f), HEX);
    logFile.println((curr_sector & 0x0f), HEX);

    mb128_rdwr_sector_num(MB128_WRITE_SECTOR, curr_sector);
    Serial.print("Sector #");
    Serial.print(((curr_sector >> 4) & 0x0f), HEX);
    Serial.println((curr_sector & 0x0f), HEX);

    for (i = 0; i < 512; i+=16 ) {
//      Serial.print(((i >> 8) & 0x0f), HEX);
//      Serial.print(((i >> 4) & 0x0f), HEX);
//      Serial.print((i & 0x0f), HEX);
//      Serial.print(": ");
      for (j = 0; j < 16; j++) {
        read_byte = dataFile.read();
        mb128_send_byte(read_byte);

        if ((read_byte < 0x20) || (read_byte > 0x7f))
          disp_char = '.';
        else
          disp_char = read_byte;
          
        char_buf[j] = disp_char;

//        Serial.print(((read_byte >> 4) & 0x0f), HEX);
//        Serial.print((read_byte & 0x0f), HEX);
//        Serial.print(" ");
      }
      for (j = 0; j < 16; j++) {
//        Serial.print(char_buf[j]);        
      }
//      Serial.println("");
    }
//    Serial.println("");
    curr_sector++;
    num_sectors--;

    logFile.println("Write transaction trailing bits");
    temp = mb128_read_bit();
    Serial.print("write_sector readbit #1 = ");
    if (temp)
      Serial.println("1");
    else
      Serial.println("0");

    temp = mb128_read_bit();
    Serial.print("write_sector readbit #2 = ");
    if (temp)
      Serial.println("1");
    else
      Serial.println("0");

    mb128_send_bit(false);  // 0
    mb128_send_bit(false);  // 0
    mb128_send_bit(false);  // 0
    logFile.println("");
  }

  return(true);
}

void write_to_mb128()
{
  dataFilePresent = false;

  Serial.println("Writing to MB128");
  
  if (!cardPresent) {
    Serial.println("No Card");
    return;
  }

  if (!SD.exists("mb128.bkp")) {
    Serial.println("mb128.bkp does not exist.");
    return;
  }

  Serial.println("Opening mb128.bkp");
  dataFile = SD.open("mb128.bkp", FILE_READ);
  dataFilePresent = true;   // I should probably have checked the result of the open...

//  Serial.println("Initialize joyport");
//  joyport_init();
//  joyport_init();
  
  Serial.println("Detect MB128");
  if (mb128_boot() == true)
    Serial.println("booted");
  else {
    Serial.println("boot failure");
    dataFile.close();
    return;
  }  

  Serial.println("Write sectors");
  if (mb128_write_sectors(0,256) == false)
    Serial.println("Error");
  else
    Serial.println("Success !");

  dataFile.close();
}

void setup()
{
int q;

  Serial.begin(500000);

  // put your setup code here, to run once:
  pinMode(clockPin, OUTPUT);
  pinMode(dataoutPin, OUTPUT);
  pinMode(d0_datainPin, INPUT);
  pinMode(d1_inPin, INPUT);
  pinMode(d2_identPin, INPUT);
  pinMode(d3_inPin, INPUT);

  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    cardPresent = false;
  }
  else {
    cardPresent = true;
    Serial.println("card initialized.");
  }

  Serial.println("Starting - delay");
  
  delay(5000);  // wait for startup period

  if (cardPresent) {
    if (SD.exists("mb128.sav")) {
      Serial.println("mb128.sav exists.");
      Serial.println("Removing mb128.sav...");
      SD.remove("mb128.sav");
    }
    if (SD.exists("mb128.log")) {
      Serial.println("mb128.log exists.");
      Serial.println("Removing mb128.log...");
      SD.remove("mb128.log");
    }

    Serial.println("Opening mb128.sav");
    dataFile = SD.open("mb128.sav", FILE_WRITE);
    logFile = SD.open("mb128.log", FILE_WRITE);
  }

  Serial.println("Initialize joyport");
  joyport_init();
  joyport_init();
  
  Serial.println("Detect MB128");
  if (mb128_boot() == true)
    Serial.println("booted");
  else {
    Serial.println("boot failure");
    if (cardPresent)
      dataFile.close();
      logFile.close();
    return;
  }

  Serial.println("Read sectors");

  time_s = millis();

  if (mb128_read_sectors(0,1) == false)
    Serial.println("Error");
  else
    Serial.println("Success !");

  time_e = millis();
  Serial.println(" ");
  Serial.print("Elapse time = ");
  Serial.print(time_e - time_s);
  Serial.println(" milliseconds");


  if (cardPresent) {
    dataFile.close();
    logFile.close();
  }
}

void loop() {
  if (Serial.available()) {
    inChar = Serial.read();
    if (inChar == 'W') {
      write_to_mb128();
    }
    else {
      Serial.print("Recieved: ");
      Serial.println(inChar);
      Serial.println("W = Write to MB128 (from MB128.BKP); R = Read from MB128 (into MB128.SAV)");
      Serial.println("Please use a valid command");
    }
  }
}
