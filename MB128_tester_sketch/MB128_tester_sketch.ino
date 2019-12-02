
#include <SD.h>

//
// This sketch implements a MB128 host
// To extract or restore the contents of the MB128 memory and perform testing
//
// This was written specifically for an Adafruit Feather M0 Adalogger, and so
// there are many dependencies which would need to be adjusted for any other
// hardware.
//

#define VERSION_NUMBER    20191201

// ------------------------
// *** HARDWARE defines ***
// ------------------------

// This sketch is made specifically for the Adafruit M0 Feather Adalogger
// We are using various pins for I/O to the MB128
//
// These are the internal mappings for those external pin identifiers
// so that we can access them directly (...and 10 times faster !)
#define IN_PORT            REG_PORT_IN0
#define OUT_PORT_SET       REG_PORT_OUTSET0
#define OUT_PORT_CLR       REG_PORT_OUTCLR0

// Inputs - raw port definitions on Feather M0 Adalogger
//
// These are used for high speed I/O, as Arduino functions
// are too slow due to library overhead
//
#define MB128_D3_INPIN     PORT_PA16    // Pin marked as D11 on M0 Feather Adalogger (D3 on PCE joypad)
#define MB128_D2_IDENTPIN  PORT_PA18    // Pin marked as D10 on M0 Feather Adalogger (D2 on PCE joypad)
#define MB128_D1_INPIN     PORT_PA20    // Pin marked as D6  on M0 Feather Adalogger (D1 on PCE joypad)
#define MB128_D0_DATAINPIN PORT_PA15    // Pin marked as D5  on M0 Feather Adalogger (D0 on PCE joypad)

// Outputs - raw port definitions on Feather M0 Adalogger
//
// These are used for high speed I/O, as Arduino functions
// are too slow due to library overhead
//
#define MB128_DATAOUTPIN   PORT_PA19    // Pin marked as D12 on M0 Feather Adalogger (SEL on PCE joypad)
#define MB128_CLOCKPIN     PORT_PA17    // Pin marked as D13 on M0 Feather Adalogger (CLR on PCE joypad)

// delay control variables
//
const bool delayMilli = false;  // if true, delay in milliseconds
const int delayShort  = 2;      // 2uS on real system
const int delayLong   = 3;      // 4uS on real system



#if defined(ADAFRUIT_FEATHER_M0)

// I/Os - these are the Arduino pin numbers
const int clockPin        = 13;  // clock to MB128 (CLR)
const int dataoutPin      = 12;  // data going to MB128 (SEL)

const int d3_inPin        = 11;  // data 3 pin from MB128
const int d2_identPin     = 10;  // data 2 - identification pin from MB128
const int d1_inPin        = 6;   // data 1 pin from MB128
const int d0_datainPin    = 5;   // data 0 - data pin from MB128 (d0)

// Chip select for on-board SDCard
const int chipSelect      = 4;

// Pins used by pushbutton switches
//
const int sw_BackupPin    = 19;
const int sw_RestorePin   = 0;
const int sw_TestPin      = 1;

// Pins used by LEDs on board(s)
//
const int led_GreenOK     = 8;
const int led_RedSDErr    = 14;
const int led_RedMB128Err = 15;
const int led_GreenRead   = 16;
const int led_YellowWrite = 17;
const int led_BlueTestOK  = 18;

#endif

const int version_num = VERSION_NUMBER;



char inChar;
boolean cardPresent;
boolean dataFilePresent;

unsigned long time_s;
unsigned long time_e;

char buffer_aa[513];
char buffer_55[513];
char buffer_mem[513];
char buffer_mem2[513];

char tempfname[16];
char lastfname[16];
char nextfname[16];
char logname[16];
int  lastfilenum = 0;
int  nextfilenum = 0;

bool debug_on = false;

File dataFile;
File logFile;

uint8_t logBuf[16];

//char to_hex(char in_nybble)
//{
//char out_char;
//  in_nybble = in_nybble & 0x0f;
//  
//  if (in_nybble < 10)
//    out_char = '0' + in_nybble;
//  else
//    out_char = 'A' - 10 + in_nybble;
//    
//  return(out_char);
//}


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

  if (debug_on) {
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
}


void mb128_send_bit(bool outbit)
{
// ***
//  following code is rapid-access for these Arduino func's:
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

  log_access('S', true, outbit);

//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;

  delay_short();

  log_access('S', false, outbit);
}

void mb128_send_byte(char outbyte)
{
bool outbit;
char i = 8;

  if (debug_on) {
    logFile.print("Send Byte 0x");
    logFile.print(((outbyte >> 4)& 0x0f), HEX);
    logFile.println((outbyte & 0x0f), HEX);
  }

  while (i > 0) {
    if (outbyte & 0x01)
      outbit = true;
    else
      outbit = false;

// ***
//  following code is rapid-access for these Arduino func's:
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
    
    log_access('S', true, outbit);

//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;
  
    delay_short();

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
//  following code is rapid-access for these Arduino func's:
//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;

//  digitalWrite(dataoutPin, LOW);
  OUT_PORT_CLR = MB128_DATAOUTPIN;
  delay_short();

//  digitalWrite(clockPin, HIGH);
  OUT_PORT_SET = MB128_CLOCKPIN;
  delay_short();

  log_access('R', true, false);

//  inbit = digitalRead(d0_datainPin);
  inbit = (IN_PORT & MB128_D0_DATAINPIN);

//  digitalWrite(clockPin, LOW);
  OUT_PORT_CLR = MB128_CLOCKPIN;
  delay_short();

  log_access('R', false, false);

  return(inbit);
}

char mb128_read_byte()
{
bool inbit;
char inbyte = 0;
char i = 8;

  if (debug_on) {
    logFile.println("Read Byte");
  }

  while (i > 0) {

// ***
//  following code is rapid-access for these Arduino func's:
//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;

//    digitalWrite(dataoutPin, LOW);
    OUT_PORT_CLR = MB128_DATAOUTPIN;

    delay_short();
    
//    digitalWrite(clockPin, HIGH);
    OUT_PORT_SET = MB128_CLOCKPIN;
    delay_short();

    log_access('R', true, false);

//    inbit = digitalRead(d0_datainPin);
    inbit = (IN_PORT & MB128_D0_DATAINPIN);

//    digitalWrite(clockPin, LOW);
    OUT_PORT_CLR = MB128_CLOCKPIN;
    delay_short();

    log_access('R', false, false);

    inbyte = inbyte >> 1;

    if (inbit == true)
      inbyte = inbyte | 0x80;
    else
      inbyte = inbyte & 0x7f;
    
    i--;
  }
  
  if (debug_on) {
    logFile.print("Byte Read = 0x");
    logFile.print(((inbyte >> 4)& 0x0f), HEX);
    logFile.println((inbyte & 0x0f), HEX);
    logFile.println();
  }

  return(inbyte);
}

void joyport_init()
{
  // simulate a joyport scan across 5 joypads
  //
  if (debug_on)
    logFile.println("joyport init");
  
  mb128_send_bit(true);
  
  if (debug_on)
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

  if (debug_on) {
    logFile.println("Detect - write 0xa8");
  }

  while (i > 0) {
    mb128_send_byte(0xa8);

    if (debug_on) {
      logFile.println("Detect - read bit #1");
    }

    mb128_send_bit(false);
    
// ***
//    if (digitalRead(d2_identPin))
    if (IN_PORT & MB128_D2_IDENTPIN)
      joy_out = joy_out | 0x40;

//    if (digitalRead(d0_datainPin))
    if (IN_PORT & MB128_D0_DATAINPIN)
      joy_out = joy_out | 0x10;
  
    if (debug_on) {
      logFile.println("Detect - read bit #2");
    }
    
    mb128_send_bit(true);

//    if (digitalRead(d2_identPin))
    if (IN_PORT & MB128_D2_IDENTPIN)
      joy_out = joy_out | 0x4;

//    if (digitalRead(d0_datainPin))
    if (IN_PORT & MB128_D0_DATAINPIN)
      joy_out = joy_out | 0x1;
  
    if (joy_out == 0x04) {
      if (debug_on)
        logFile.println("");

      return(true);
    }
    else {
      Serial.print("0xA8 not acknowledged... ");
      Serial.print("joy_out = ");
      Serial.println(joy_out, HEX);

      if (debug_on) {
        logFile.print("0xA8 not acknowledged... ");
        logFile.print("joy_out = ");
        logFile.println(joy_out, HEX);
      }
    }
  
    joy_out = 0;
    i = i -1;
  }

  // send three zero bits if it was not detected
  if (debug_on) {
    logFile.println("Detect - send 3 trailing bits");
  }

  mb128_send_bit(false);
  mb128_send_bit(false);
  mb128_send_bit(false);

  if (debug_on) {
    logFile.println("");
  }
  
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

  if (debug_on) {
    logFile.println("Boot lead");
  }
  
  mb128_send_bit(true);   // 1
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  
  if (debug_on) {
    logFile.println("Boot Byte 0x00");
  }
  
  mb128_send_byte(0x00);  // 0x00
  
  if (debug_on) {
    logFile.println("Boot Byte 0x01");
  }

  mb128_send_byte(0x01);  // 0x01
  
  if (debug_on) {
    logFile.println("Boot Byte 0x00");
  }

  mb128_send_byte(0x00);  // 0x00
  
  if (debug_on) {
    logFile.println("Boot 4 bits");
  }

  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  
  if (debug_on) {
    logFile.println("Boot readbit");
  }

  temp = mb128_read_bit();

//  Serial.print("boot readbit = ");
//  if (temp)
//    Serial.println("1");
//  else
//    Serial.println("0");
  
  if (debug_on) {
    logFile.println("Boot trail 3 bits");
  }

  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0

  if (debug_on) {
    logFile.println("");
  }

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

//TODO: Fix this section to explain write of length properly
//
  logFile.println("Read-write length");
  mb128_send_byte(0x00);  // 0x00
  mb128_send_byte(0x10);  // 0x10
  
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0
  mb128_send_bit(false);  // 0  
  logFile.println("");
}

// set start addr
void mb128_rdwr_addr_len(bool rdwr, int start_addr, long bytes, char bits)
{
  int i;
  long bitmask;
  
  logFile.println("Read-write header");
  mb128_send_bit(rdwr);   // 1 = read; 0 = write

  logFile.println("Read-write address");  // sent as multiples of 128 bytes
  bitmask = 1;
  for (i = 0; i < 10; i++) {
    
    if (start_addr & bitmask) {
      mb128_send_bit(true);
    }
    else {
      mb128_send_bit(false);
    }
    bitmask <<= 1;
  }

  // length is first sent as number of bits, fllowed by number of bytes
  logFile.println("Number of bits");
  bitmask = 1;
  for (i = 0; i < 3; i++) {
    
    if (bits & bitmask) {
      mb128_send_bit(true);
    }
    else {
      mb128_send_bit(false);
    }
    bitmask <<= 1;
  }
  
  logFile.println("Number of bytes");
  bitmask = 1;
  for (i = 0; i < 17; i++) {
    
    if (bytes & bitmask) {
      mb128_send_bit(true);
    }
    else {
      mb128_send_bit(false);
    }
    bitmask <<= 1;
  }

  logFile.println("");
}

void mb128_read_to_buffer_512(char * bufptr)
{
int i, j;
char read_byte;
char disp_char;
char char_buf[16];

   for (i = 0; i < 512; i+=16 ) {
//     Serial.print(((i >> 8) & 0x0f), HEX);
//     Serial.print(((i >> 4) & 0x0f), HEX);
//     Serial.print((i & 0x0f), HEX);
//     Serial.print(": ");
     for (j = 0; j < 16; j++) {
       read_byte = mb128_read_byte();

       // put it into the return buffer
       *(bufptr) = read_byte;
       bufptr++;
       
       if ((read_byte < 0x20) || (read_byte > 0x7f))
         disp_char = '.';
       else
         disp_char = read_byte;
          
       char_buf[j] = disp_char;

//       Serial.print(((read_byte >> 4) & 0x0f), HEX);
//       Serial.print((read_byte & 0x0f), HEX);
//       Serial.print(" ");
     }
     for (j = 0; j < 16; j++) {
//       Serial.print(char_buf[j]);        
     }
//     Serial.println("");
   }
}

bool mb128_read_sector(char sector, char *bufptr)
{
   if (!mb128_detect())
     return(false);

   digitalWrite(led_GreenOK, LOW);
   digitalWrite(led_GreenRead, HIGH);

   mb128_rdwr_sector_num(MB128_READ_SECTOR, sector);
   mb128_read_to_buffer_512(bufptr);

   digitalWrite(led_GreenRead, LOW);
   digitalWrite(led_GreenOK, HIGH);

// trailing bits (3 bits for reads)
   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0
   
   return(true);
}

bool mb128_read_sectors(char start_sector, int num_sectors)
{
int i, j;
char read_byte;
char disp_char;
char curr_sector = start_sector;
char char_buf[16];
char read_buf[512];

//NOTE: Where multiple sectors are read, each sector is requsted one-by-one
//      rather than using a single large read
//
  while (num_sectors > 0) {

    logFile.print("Read sector 0x");
    logFile.print(((curr_sector >> 4)& 0x0f), HEX);
    logFile.println((curr_sector & 0x0f), HEX);

    Serial.print("Sector #");
    Serial.print(((curr_sector >> 4) & 0x0f), HEX);
    Serial.println((curr_sector & 0x0f), HEX);

    if (!mb128_read_sector(curr_sector, read_buf))
      return(false);

    if (cardPresent)
      dataFile.write(read_buf, 512);

    curr_sector++;
    num_sectors--;
    delay(1);
  }

  return(true);
}

void mb128_write_from_buffer_512(char * bufptr)
{
int i, j;
char read_byte;
char disp_char;
char char_buf[16];

   for (i = 0; i < 512; i+=16 ) {
//     Serial.print(((i >> 8) & 0x0f), HEX);
//     Serial.print(((i >> 4) & 0x0f), HEX);
//     Serial.print((i & 0x0f), HEX);
//     Serial.print(": ");

     for (j = 0; j < 16; j++) {
       read_byte = *(bufptr);
       mb128_send_byte(read_byte);
       bufptr++;

       if ((read_byte < 0x20) || (read_byte > 0x7f))
         disp_char = '.';
       else
         disp_char = read_byte;
         
       char_buf[j] = disp_char;

//       Serial.print(((read_byte >> 4) & 0x0f), HEX);
//       Serial.print((read_byte & 0x0f), HEX);
//       Serial.print(" ");
     }
     for (j = 0; j < 16; j++) {
//       Serial.print(char_buf[j]);        
     }
//     Serial.println("");
   }
}

bool mb128_write_sector(char sector, char *bufptr)
{
bool temp1, temp2;
int sector_num;

   if (!mb128_detect()) {
     Serial.println("Write_sector - failed to detect");
     return(false);
   }

   digitalWrite(led_GreenOK, LOW);
   digitalWrite(led_YellowWrite, HIGH);

   mb128_rdwr_sector_num(MB128_WRITE_SECTOR, sector);
   mb128_write_from_buffer_512(bufptr);

   digitalWrite(led_YellowWrite, LOW);
   digitalWrite(led_GreenOK, HIGH);

// trailing bits (3 + 2 bits for writes):

   temp1 = mb128_read_bit();
   temp2 = mb128_read_bit();

   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0

// post-write bit #1 indicates device type:
// 0 = save-kun
// 1 = memory base 128

//   if (!temp1) {
//     logFile.print("Write sector 0x");
//     sector_num = sector;
//     logFile.print(sector_num);
//     logFile.println(", sector write operation, post-sector bit#1 != 1... Fail"); 
//     Serial.println("Write_sector - post-sector bit #1 != 1 - Fail");
//     return(false);
//   }

// post-write bit #2 was thought to indicate success/failure, but it
// appears not to be the case, based on MooZ's experience with a Save-kun
//
//   if (temp2) {
//     logFile.print("Write sector 0x");
//     sector_num = sector;
//     logFile.print(sector_num);
//     logFile.println(", sector write operation, post-sector bit#2 != 0... Fail"); 
//     Serial.println("Write_sector - post-sector bit #2 != 0 - Fail");
//     return(false);
//   }

   return(true);
}

bool mb128_write_sectors(char start_sector, int num_sectors)
{
int i, j;
char read_byte;
char disp_char;
char curr_sector = start_sector;
char write_buf[512];
char char_buf[16];
bool temp;


//  if (!dataFilePresent) {
//    return(false);
//  }

  while (num_sectors > 0) {

    dataFile.readBytes(write_buf, 512);

    logFile.print("Write sector 0x");
    logFile.print(((curr_sector >> 4)& 0x0f), HEX);
    logFile.println((curr_sector & 0x0f), HEX);

    Serial.print("Sector #");
    Serial.print(((curr_sector >> 4) & 0x0f), HEX);
    Serial.println((curr_sector & 0x0f), HEX);

    if (!mb128_write_sector(curr_sector, write_buf))
      return(false);

    curr_sector++;
    num_sectors--;
    delay(1);
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

void flourish()
{
  digitalWrite(clockPin, HIGH);
  digitalWrite(led_GreenOK, HIGH);
  digitalWrite(led_RedSDErr, HIGH);
  digitalWrite(led_RedMB128Err, HIGH);
  digitalWrite(led_GreenRead, HIGH);
  digitalWrite(led_YellowWrite, HIGH);
  digitalWrite(led_BlueTestOK, HIGH);
  delay(600);

  digitalWrite(clockPin, LOW);
  digitalWrite(led_GreenOK, LOW);
  digitalWrite(led_RedSDErr, LOW);
  digitalWrite(led_RedMB128Err, LOW);
  digitalWrite(led_GreenRead, LOW);
  digitalWrite(led_YellowWrite, LOW);
  digitalWrite(led_BlueTestOK, LOW);
  delay(180);

  digitalWrite(led_BlueTestOK, HIGH);
  delay(180);
  digitalWrite(led_BlueTestOK, LOW);

  digitalWrite(led_YellowWrite, HIGH);
  delay(180);
  digitalWrite(led_YellowWrite, LOW);

  digitalWrite(led_GreenRead, HIGH);
  delay(180);
  digitalWrite(led_GreenRead, LOW);

  digitalWrite(led_RedMB128Err, HIGH);
  delay(180);
  digitalWrite(led_RedMB128Err, LOW);

  digitalWrite(led_RedSDErr, HIGH);
  delay(180);
  digitalWrite(led_RedSDErr, LOW);

  delay(200);

  digitalWrite(led_GreenOK, HIGH);
}

// flash_error is a terminal state - it blinks forever
//             (until reset is pressed)
//
void flash_error(int led)
{
  digitalWrite(led_GreenOK, LOW);
  
  while (1) {
    digitalWrite(led, HIGH);
    delay(200);
    digitalWrite(led, LOW);
    delay(200);
  }
}

void blink_error(int led)
{
  int i;
  
  digitalWrite(led_GreenOK, LOW);
  
  for (i = 0; i < 5; i++) {
    digitalWrite(led, HIGH);
    delay(100);
    digitalWrite(led, LOW);
    delay(100);
  }
  digitalWrite(led_GreenOK, HIGH);
}

void find_filename()
{
  int i, j;

  lastfilenum = -1;

  digitalWrite(led_GreenOK, LOW);
  digitalWrite(led_BlueTestOK, LOW);
  digitalWrite(led_GreenRead, HIGH);

  // Need to re-initialize each time the card might be replaced
  //
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed or not present");
    flash_error(led_RedSDErr);
  }

  if (SD.exists("mb128.dbg"))
    debug_on = true;
  else
    debug_on = false;

  // Since scanning for 100 files is time-consuming,
  // check 10 by 10, then one by one:
  //
  for (i = 0; i < 100; i+=10) {

    lastfname[6] = (i/10) + '0';
    lastfname[7] = (i%10) + '0';
    
//    Serial.print("Trying ");
//    Serial.print(lastfname);
    
    if (SD.exists(lastfname)) {
//      Serial.println(" - found");
      lastfilenum = i;
      
    } else {
//      Serial.println(" - not found");
      break;
    }
  }

  j = lastfilenum;

  // If at least one file (mb128_00.bkp) was found,
  // then we can search one by one:
  //
  if (j > -1) {
    for (i = j+1; i < 100; i++) {

      lastfname[6] = (i/10) + '0';
      lastfname[7] = (i%10) + '0';

//      Serial.print("Trying ");
//      Serial.print(lastfname);
    
      if (SD.exists(lastfname)) {
//        Serial.println(" - found");
        lastfilenum = i;
      
      } else {
//        Serial.println(" - not found");
        break;
      }
    }
  }
  
  lastfname[6] = (lastfilenum/10) + '0';
  lastfname[7] = (lastfilenum%10) + '0';

  if (lastfilenum < 99) {
    nextfilenum = lastfilenum + 1;
    nextfname[6] = (nextfilenum/10) + '0';
    nextfname[7] = (nextfilenum%10) + '0';
  }
  else
    nextfilenum = -1;

  Serial.println("Filename = mb128_xx.sav");
  Serial.print("  lastfilenum (for restore) = ");
  Serial.println(lastfilenum);
  Serial.print("  nextfilenum (for backup) = ");
  Serial.println(nextfilenum);

  digitalWrite(led_GreenRead, LOW);
  digitalWrite(led_GreenOK, HIGH);
}

void setup()
{
int i;
int q;

  Serial.begin(19200);

  for (i = 0; i < 512; i++)
  {
    buffer_aa[i] = 0xaa;
    buffer_55[i] = 0x55;
  }

  // initialize I/O pins:
  //
  pinMode(clockPin, OUTPUT);
  pinMode(dataoutPin, OUTPUT);
  pinMode(d0_datainPin, INPUT);
  pinMode(d1_inPin, INPUT);
  pinMode(d2_identPin, INPUT);
  pinMode(d3_inPin, INPUT);

  pinMode(sw_BackupPin, INPUT_PULLUP);
  pinMode(sw_RestorePin, INPUT_PULLUP);
  pinMode(sw_TestPin, INPUT_PULLUP);

  pinMode(led_GreenOK, OUTPUT);
  pinMode(led_RedSDErr, OUTPUT);
  pinMode(led_RedMB128Err, OUTPUT);
  pinMode(led_GreenRead, OUTPUT);
  pinMode(led_YellowWrite, OUTPUT);
  pinMode(led_BlueTestOK, OUTPUT);

  // wait for startup, then flash LEDs
  // to indicate boot in progress
  //
  delay(4000);  // wait for startup period
  flourish();

  // initialize SDCard filenames
  //
  strcpy(lastfname, "mb128_xx.sav");
  strcpy(nextfname, "mb128_xx.sav");
  strcpy(logname,   "mb128_xx.log");
  strcpy(tempfname, "temp128.bkp");


  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    cardPresent = false;
   
    flash_error(led_RedSDErr);
  }
  else {
    cardPresent = true;
    Serial.println("card initialized.");
  }

//  Serial.println("Starting - delay");

//
//TODO - make the startup debug code conditional on the debug
//

  find_filename();
  
// ******  debug_on = true;   ******


  Serial.println();
  
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
    
    flash_error(led_RedMB128Err);

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

  Serial.println("");
  Serial.println("");
  Serial.print("*** MB128 Tester, Version ");
  Serial.print(version_num);
  Serial.println(" ***");
  Serial.println("Ready");
  Serial.println("");

  if (cardPresent) {
    dataFile.close();
    logFile.close();
  }
}

//**************
// BACKUP BUTTON
//**************
void backup_button()
{
    Serial.println("\nBackup button pressed");
    digitalWrite(led_GreenOK, LOW);
    digitalWrite(led_BlueTestOK, LOW);

    find_filename();
    logname[6] = nextfname[6];
    logname[7] = nextfname[7];

    // If mb128_99.bkp already in use:
    //
    if (nextfilenum == -1) {
      Serial.println("SDCard already contains 99 backup files !!");
      blink_error(led_RedSDErr);
      digitalWrite(led_GreenOK, HIGH);
      delay(1000);
      return;
    }

    // Check for duplicate files (should not happen !!)
    //
    if (debug_on) {
      if (SD.exists(logname)) {
        Serial.print("Error - ");
        Serial.print(logname);
        Serial.println(" exists.");

        flash_error(led_RedSDErr);
      }
    }

    // Check for duplicate files (should not happen !!)
    //
    if (SD.exists(nextfname)) {
      Serial.print("Error - ");
      Serial.print(nextfname);
      Serial.println(" exists.");
      flash_error(led_RedSDErr);
    }

    // otherwise, OK to open file(s)
    Serial.print("Opening datafile: ");
    Serial.println(nextfname);
    dataFile = SD.open(nextfname, FILE_WRITE);

    if (!dataFile) {
      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
    if (debug_on) {
      Serial.print("Opening logfile: ");
      Serial.println(logname);
      logFile = SD.open(logname, FILE_WRITE);
      
      if (!logFile) {
        Serial.print("Error Opening logfile.");
        flash_error(led_RedSDErr);
      }
    }

    //
    // DO actual work in here:
    //
    if (mb128_boot() != true) {

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_read_sectors(0,256) == false) {
      
      Serial.println("Error while reading from MB128");
      
      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    } 

    Serial.println("Read completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

    digitalWrite(led_GreenOK, HIGH);
    delay(600);
}

//***************
// RESTORE BUTTON
//***************
//
void restore_button()
{
  char sector;
  int  sector_num;
  bool test_OK;

    test_OK = true;
    
    Serial.println("\nRestore Button Pressed");
    digitalWrite(led_GreenOK, LOW);
    digitalWrite(led_BlueTestOK, LOW);
    
    find_filename();

    logname[6] = lastfname[6];
    logname[7] = lastfname[7];

    // If mb128_99.bkp already in use:
    //
    if (lastfilenum == -1) {
      Serial.println("No Backup File on SDCard to Restore");
      blink_error(led_RedSDErr);
      digitalWrite(led_GreenOK, HIGH);
      delay(1000);
      return;
    }

    // Check for duplicate files (should not happen !!)
    //
    if (debug_on) {
      if (SD.exists(logname)) {
        Serial.println("Overwriting logfile");
        SD.remove(logname);
      }
    }

    // Check for duplicate files (should not happen !!)
    //
    if (!SD.exists(lastfname)) {
      Serial.print("Error - ");
      Serial.print(lastfname);
      Serial.println(" doesn't exist.");
      flash_error(led_RedSDErr);
    }

    // otherwise, OK to open file(s)
    Serial.print("Opening datafile: ");
    Serial.println(lastfname);
    dataFile = SD.open(lastfname, FILE_READ);

    if (!dataFile) {
      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
    if (debug_on) {
      Serial.print("Opening logfile: ");
      Serial.println(logname);
      logFile = SD.open(logname, FILE_WRITE);
      
      if (!logFile) {
        Serial.print("Error Opening logfile.");
        flash_error(led_RedSDErr);
      }
    }

    //
    // DO actual work in here:
    //
    if (mb128_boot() != true) {

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    for (sector_num = 0; sector_num < 256; sector_num++)
    {
      sector = sector_num;

      dataFile.readBytes(buffer_mem, 512);  // This reads next sector, not absolute sector

      logFile.print("Write sector 0x");
      logFile.print(((sector_num >> 4)& 0x0f), HEX);
      logFile.println((sector_num & 0x0f), HEX);

      Serial.print("Sector #");
      Serial.print(((sector_num >> 4) & 0x0f), HEX);
      Serial.print((sector_num & 0x0f), HEX);


      Serial.print(" - write");
      if (!mb128_write_sector(sector, buffer_mem)) {
        Serial.println(" - sector failed (1)");
        test_OK = false;
        break;
      }
      Serial.print("|verify");
      if (!mb128_read_sector(sector, buffer_mem2)) {
        Serial.println(" - sector failed (2)");
        test_OK = false;
        break;
      }
      
      if (memcmp(buffer_mem, buffer_mem2, 512) != 0) {
        Serial.println(" - sector failed (3)");
        test_OK = false;
        break;
      }
      Serial.println("");
    }

    if (test_OK == false) {

      Serial.println("Error while writing to MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    Serial.println("Write completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

    // if restore() == OK
    digitalWrite(led_GreenOK, HIGH);
    delay(600);
}

//*************
// TEST BUTTON
//*************
//
void test_button()
{
  char sector;
  int sector_num;
  bool test_OK;

   Serial.println("\nTest Button Pressed");
   digitalWrite(led_GreenOK, LOW);
   digitalWrite(led_BlueTestOK, LOW);

   test_OK = true;
    
   // Check for a file called testtemp.bkp
   // if found, remove
   // make file

   if (cardPresent) {
      if (SD.exists("testtemp.bkp")) {
         Serial.println("testtemp.bkp exists.");
         Serial.println("Removing testtemp.bkp...");
         SD.remove("testtemp.bkp");
       }
       if ((debug_on) && (SD.exists("mb128.log"))) {
         Serial.println("mb128.log exists.");
         Serial.println("Removing mb128.log...");
         SD.remove("mb128.log");
       }

      Serial.println("Saving backup to testtemp.bkp");
      dataFile = SD.open("testtemp.bkp", FILE_WRITE);
   }

    if (!dataFile) {
      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
    if (debug_on) {
      Serial.print("Opening logfile: ");
      Serial.println("mb128.log");
      logFile = SD.open("mb128.log", FILE_WRITE);
      
      if (!logFile) {
        Serial.print("Error Opening logfile.");
        flash_error(led_RedSDErr);
      }
    }

    //
    // DO actual work in here:
    //
    if (mb128_boot() != true) {

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_read_sectors(0,256) == false) {
      
      Serial.println("Error while reading from MB128");
      
      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    } 

    Serial.println("Read completed");
    Serial.println("");
    dataFile.close();

    // then for sector from 0 to 127:
    //   write AA; read sector; compare value -> if bad, then fail
    //   write 55; read sector; compare value -> if bad then fail

    for (sector_num = 0; sector_num < 256; sector_num++)
    {
      sector = sector_num;
      Serial.print("Sector #");
      Serial.print(((sector_num >> 4) & 0x0f), HEX);
      Serial.print((sector_num & 0x0f), HEX);

      Serial.print(" - AA:write");

      if (!mb128_write_sector(sector, buffer_aa)) {
        Serial.println(" - sector failed (1)");
        test_OK = false;
        break;
      }
      Serial.print("|verify");
      if (!mb128_read_sector(sector, buffer_mem)) {
        Serial.println(" - sector failed (2)");
        test_OK = false;
        break;
      }
      
      if (memcmp(buffer_aa, buffer_mem, 512) != 0) {
        Serial.println(" - sector failed (3)");
        test_OK = false;
        break;
      }
      
      Serial.print(" || 55:write");
      if (!mb128_write_sector(sector, buffer_55)) {
        Serial.println(" - sector failed (4)");
        test_OK = false;
        break;
      }
      Serial.print("|verify");
      if (!mb128_read_sector(sector, buffer_mem)) {
        Serial.println(" - sector failed (5)");
        test_OK = false;
        break;
      }
      
      if (memcmp(buffer_55, buffer_mem, 512) != 0) {
        Serial.println("sector failed (6)");
        test_OK = false;
        break;
      }
      Serial.println(" -- OK");
    }

    if (test_OK == false) {
      digitalWrite(led_RedMB128Err, HIGH);
      flash_error(led_RedMB128Err);
    }

    Serial.println("");
    Serial.println("*** Test Successful ***");
    Serial.println("");

    // Now, restore from testtemp
    // Light TEST OK if OK
    Serial.print("Restoring from backup datafile: ");
    Serial.println("testtemp.bkp");
    dataFile = SD.open("testtemp.bkp", FILE_READ);

    if (!dataFile) {
      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
// Restore original data

    if (mb128_boot() != true) {

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_write_sectors(0,256) == false) {
      
      Serial.println("Error while writing to MB128");
      
      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    } 

    Serial.println("Restore completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

    // if restore() == OK
    digitalWrite(led_GreenOK, HIGH);
    digitalWrite(led_BlueTestOK, HIGH);
    delay(600);
}



void loop() {
  int rdval;

  if (digitalRead(sw_BackupPin) == LOW) {
    backup_button();
  }

  if (digitalRead(sw_RestorePin) == LOW) {
    restore_button();
  }

  if (digitalRead(sw_TestPin) == LOW) {
    test_button();
  }

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
