
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>
#include <SD.h>

//
// This sketch implements a MB128 host
// To extract or restore the contents of the MB128 memory and perform testing
//
// This was written specifically for an Adafruit Feather M0 Adalogger, and 
// is currnetly in the process of being converted to a Wio Terminal.
// There are many dependencies which need to be adjusted.
//

#define VERSION_NUMBER    20210113

// ------------------------
// *** HARDWARE defines ***
// ------------------------


#if defined(_VARIANT_WIO_TERMINAL)

// This sketch was originally made specifically for the Adafruit M0 Feather Adalogger
// but has been migrated to the Seeedstudio Wio Terminal
//
// We are using various pins for I/O to the MB128
//
// These are the internal mappings for those external pin identifiers
// so that we can access them directly (...and 10 times faster !)
//
// Note that the PBxx ports are on register "1"
// (PAxx ports would require 'REG_PORT_IN0' for example)
//
#define IN_PORT            REG_PORT_IN1
#define OUT_PORT_SET       REG_PORT_OUTSET1
#define OUT_PORT_CLR       REG_PORT_OUTCLR1

// Inputs - raw port definitions on Wio Terminal
//
// These are used for high speed I/O, as Arduino functions
// are too slow due to library overhead
//
#define MB128_D3_INPIN     PORT_PB04    // Pin marked as D3 on Wio Terminal (D3 on PCE joypad)
#define MB128_D2_IDENTPIN  PORT_PB05    // Pin marked as D4 on Wio Terminal (D2 on PCE joypad)
#define MB128_D1_INPIN     PORT_PB06    // Pin marked as D5 on Wio Terminal (D1 on PCE joypad)
#define MB128_D0_DATAINPIN PORT_PB07    // Pin marked as D7 on Wio Terminal (D0 on PCE joypad)

// Outputs - raw port definitions on Wio Terminal
//
// These are used for high speed I/O, as Arduino functions
// are too slow due to library overhead
//
#define MB128_DATAOUTPIN   PORT_PB08    // Pin marked as D0 on Wio Terminal (SEL on PCE joypad)
#define MB128_CLOCKPIN     PORT_PB09    // Pin marked as D1 on Wio Terminal (CLR on PCE joypad)

#endif

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

//#define FAST_ACCESS

// delay control variables
//
const bool delayMilli = false;  // if true, delay in milliseconds
const int delayShort  = 2;      // 2uS on real system
const int delayLong   = 3;      // 3uS on real system


#if defined(_VARIANT_WIO_TERMINAL)
 
// I/Os - these are the Arduino pin numbers
const int clockPin        = D1;  // clock to MB128 (CLR)
const int dataoutPin      = D0;  // data going to MB128 (SEL)

const int d3_inPin        = D3;  // data 3 pin from MB128
const int d2_identPin     = D4;  // data 2 - identification pin from MB128
const int d1_inPin        = D5;   // data 1 pin from MB128
const int d0_datainPin    = D7;   // data 0 - data pin from MB128 (d0)

// Chip select for on-board SDCard
const int chipSelect      = SDCARD_SS_PIN;


// Pins used by pushbutton switches
//
const int sw_BackupPin    = WIO_KEY_C;    // leftmost button
const int sw_RestorePin   = WIO_KEY_B;    // middle button
const int sw_TestPin      = WIO_KEY_A;    // right button

// Pins used by LEDs on board(s)
//

// CHANGE THESE LATER !!!!!
const int led_GreenOK     = D2;
const int led_RedSDErr    = D2;
const int led_RedMB128Err = D2;
const int led_GreenRead   = D2;
const int led_YellowWrite = D2;
const int led_BlueTestOK  = D2;

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

  // length is first sent as number of bits, followed by number of bytes
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

   mb128_rdwr_sector_num(MB128_READ_SECTOR, sector);
   mb128_read_to_buffer_512(bufptr);

// trailing bits (3 bits for reads)
   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0
   mb128_send_bit(false);  // 0
   
   return(true);
}

bool mb128_read_sectors(char start_sector, int num_sectors, bool draw_text)
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

    if (draw_text) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Reading Block #", 10, 120, 4);
  
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      sprintf(char_buf,"%d",curr_sector);
      tft.drawString(char_buf, 210, 120, 4);
    }

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

   mb128_rdwr_sector_num(MB128_WRITE_SECTOR, sector);
   mb128_write_from_buffer_512(bufptr);

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

bool mb128_write_sectors(char start_sector, int num_sectors, bool draw_text)
{
int i, j;
char read_byte;
char disp_char;
char curr_sector = start_sector;
char write_buf[512];
char char_buf[50];
bool temp;


//  if (!dataFilePresent) {
//    return(false);
//  }

  while (num_sectors > 0) {

    if (draw_text) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      sprintf(char_buf,"Restoring Block # %d",curr_sector);
      tft.drawString(char_buf, 10, 120, 4);
    }
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
  if (mb128_write_sectors(0,256, false) == false)
    Serial.println("Error");
  else
    Serial.println("Success !");

  dataFile.close();
}

// flash_error is a terminal state - it blinks forever
//             (until reset is pressed)
//
void flash_error(int led)
{
  while (1) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Error", 10, 220, 2);
    delay(200);
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("Error", 10, 220, 2);
    delay(200);
  }
}

void blink_error(int led)
{
  int i;
  
  for (i = 0; i < 5; i++) {
    delay(100);
    delay(100);
  }
}

void find_filename()
{
  int i, j;

  lastfilenum = -1;


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

}

void wait_screen()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawString("Save", 0, 0, 4);
  tft.setTextColor(TFT_BLACK, TFT_RED);
  tft.drawString("Restore", 58, 0, 4);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.drawString("Test", 148, 0, 4);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Save:", 5, 70, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Store the MB128 contents to SDCard", 65, 70, 2);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Restore:", 5, 110, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Restore last backup to the MB128", 65, 110, 2);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Test:", 5, 150, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Perform a backup of the MB128,", 65, 150, 2);
  tft.drawString("then run a series of tests on it,", 65, 170, 2);
  tft.drawString("and restore the original contents", 65, 190, 2);
}

void process_screen()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_DARKGREY);
  tft.drawString("Save", 0, 0, 4);
  tft.setTextColor(TFT_BLACK, TFT_DARKGREY);
  tft.drawString("Restore", 58, 0, 4);
  tft.setTextColor(TFT_BLACK, TFT_DARKGREY);
  tft.drawString("Test", 148, 0, 4);

  tft.fillRect(0, 60, 320, 180, TFT_BLACK);
}

void setup()
{
int i;
int q;
char version[20];

  tft.init();
  tft.setRotation(3);
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

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

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawCentreString("PC Engine", 160, 70, 4);
  tft.drawCentreString("Memory Base 128", 160, 100, 4);
  tft.drawCentreString("Utility", 160, 130, 4);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Version ", 10, 220, 2);
  sprintf(version, "%d", VERSION_NUMBER);
  tft.drawString(version, 60, 220, 2);
  
  // wait for startup, then eexecute startup process
  // to indicate boot in progress
  //
  delay(4500);  // wait for startup period
  tft.fillScreen(TFT_BLACK);

  // initialize SDCard filenames
  //
  strcpy(lastfname, "mb128_xx.sav");
  strcpy(nextfname, "mb128_xx.sav");
  strcpy(logname,   "mb128_xx.log");
  strcpy(tempfname, "temp128.bkp");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Checking SD card...", 10, 60, 2);

  Serial.print("Initializing SD card...");
  delay(500);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {

    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("Card failed or not present", 10, 90, 4);

    Serial.println("Card failed, or not present");
    // don't do anything more:
    cardPresent = false;
   
    flash_error(led_RedSDErr);
  }
  else {
    cardPresent = true;
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Card initialized.", 150, 60, 2);
    Serial.println("card initialized.");
  }

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

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Initialize joyport", 10, 90, 2);

  Serial.println("Initialize joyport");
  joyport_init();
  joyport_init();
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Detect MB128...", 10, 120, 2);

  Serial.println("Detect MB128");
  if (mb128_boot() == true) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("OK", 150, 120, 2);

    Serial.println("booted");
  } else {

    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("MB128 failed or not present", 10, 150, 4);

    Serial.println("boot failure");

    if (cardPresent) {
      dataFile.close();
      logFile.close();
    }
    flash_error(led_RedMB128Err);
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Read sector - test", 10, 150, 2);

  Serial.println("Read sector - test");

  time_s = millis();

  if (mb128_read_sectors(0,1, false) == false) {
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("Read sector failure", 10, 180, 4);

    Serial.println("Error");
  }
  else
  {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("OK", 150, 150, 2);

    Serial.println("Success !");
  }

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
  delay(2000);

  wait_screen();
}

//**************
// BACKUP BUTTON
//**************
void backup_button()
{
char errmsg[50];

    Serial.println("\nBackup button pressed");

    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.drawString("Backup", 10, 45, 4);

    find_filename();
    logname[6] = nextfname[6];
    logname[7] = nextfname[7];

    // If mb128_99.bkp already in use:
    //
    if (nextfilenum == -1) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("SDCard already contains 99 backup files", 10, 120, 2);

      Serial.println("SDCard already contains 99 backup files !!");
      blink_error(led_RedSDErr);
      delay(1000);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("SDCard already contains 99 backup files", 10, 120, 2);
      return;
    }

    // Check for duplicate files (should not happen !!)
    //
    if (debug_on) {
      if (SD.exists(logname)) {
        sprintf(errmsg, "Error - %s exists.", logname);
        tft.setTextColor(TFT_BLACK, TFT_RED);
        tft.drawString(errmsg, 10, 120, 2);

        Serial.print("Error - ");
        Serial.print(logname);
        Serial.println(" exists.");

        flash_error(led_RedSDErr);
      }
    }

    // Check for duplicate files (should not happen !!)
    //
    if (SD.exists(nextfname)) {
      sprintf(errmsg, "Error - %s exists.", logname);
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString(errmsg, 10, 120, 2);

      Serial.print("Error - ");
      Serial.print(nextfname);
      Serial.println(" exists.");
      flash_error(led_RedSDErr);
    }

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    sprintf(errmsg, "Saving to file: %s", nextfname);
    tft.drawString(errmsg, 10, 90, 2);

    // otherwise, OK to open file(s)
    Serial.print("Opening datafile: ");
    Serial.println(nextfname);
    dataFile = SD.open(nextfname, FILE_WRITE);

    if (!dataFile) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error Opening datafile.", 10, 120, 2);

      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
    if (debug_on) {
      Serial.print("Opening logfile: ");
      Serial.println(logname);
      logFile = SD.open(logname, FILE_WRITE);
      
      if (!logFile) {
        tft.setTextColor(TFT_BLACK, TFT_RED);
        tft.drawString("Error Opening logfile.", 10, 120, 2);

        Serial.print("Error Opening logfile.");
        flash_error(led_RedSDErr);
      }
    }

    //
    // DO actual work in here:
    //
    if (mb128_boot() != true) {

      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Failed to recognize MB128", 10, 120, 2);

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_read_sectors(0,256, true) == false) {
      
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error while reading from MB128", 10, 180, 2);

      Serial.println("Error while reading from MB128");
      
      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    } 

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Read completed", 10, 180, 2);

    Serial.println("Read completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

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
  char errmsg[50];
  char char_buf[50];


    test_OK = true;
    
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.drawString("RESTORE", 10, 45, 4);

    Serial.println("\nRestore Button Pressed");
    
    find_filename();

    logname[6] = lastfname[6];
    logname[7] = lastfname[7];

    // If mb128_99.bkp already in use:
    //
    if (lastfilenum == -1) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("No MB128 Backup File on SDCard", 10, 120, 2);

      Serial.println("No Backup File on SDCard to Restore");
      blink_error(led_RedSDErr);
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
      sprintf(errmsg, "Error - %s doesn't exist.", lastfname);
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString(errmsg, 10, 120, 2);

      Serial.print("Error - ");
      Serial.print(lastfname);
      Serial.println(" doesn't exist.");
      flash_error(led_RedSDErr);
    }

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    sprintf(errmsg, "Restoring from file: %s", nextfname);
    tft.drawString(errmsg, 10, 90, 2);

    // otherwise, OK to open file(s)
    Serial.print("Opening datafile: ");
    Serial.println(lastfname);
    dataFile = SD.open(lastfname, FILE_READ);

    if (!dataFile) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error Opening datafile.", 10, 120, 2);

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

      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Failed to recognize MB128", 10, 120, 2);

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

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Block #", 10, 120, 4);
  
      sprintf(char_buf,"%d",sector);
      tft.drawString(char_buf, 100, 120, 4);

      dataFile.readBytes(buffer_mem, 512);  // This reads next sector, not absolute sector

      logFile.print("Write sector 0x");
      logFile.print(((sector_num >> 4)& 0x0f), HEX);
      logFile.println((sector_num & 0x0f), HEX);

      Serial.print("Sector #");
      Serial.print(((sector_num >> 4) & 0x0f), HEX);
      Serial.print((sector_num & 0x0f), HEX);

      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Write", 155, 120, 4);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Verify", 230, 120, 4);

      Serial.print(" - write");
      if (!mb128_write_sector(sector, buffer_mem)) {
        Serial.println(" - sector failed (1)");
        test_OK = false;
        break;
      }

      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Write", 155, 120, 4);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("Verify", 230, 120, 4);

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

      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error while writing to MB128", 10, 180, 2);

      Serial.println("Error while writing to MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Write completed", 10, 180, 2);
    
    Serial.println("Write completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

    // if restore() == OK
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
  char errmsg[50];
  char char_buf[50];

   tft.setTextColor(TFT_BLACK, TFT_YELLOW);
   tft.drawString("TEST", 10, 45, 4);

   Serial.println("\nTest Button Pressed");

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

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Saving to file: testtemp.bkp", 10, 90, 2);

    if (!dataFile) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error Opening datafile.", 10, 120, 2);

      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
    if (debug_on) {
      Serial.print("Opening logfile: ");
      Serial.println("mb128.log");
      logFile = SD.open("mb128.log", FILE_WRITE);
      
      if (!logFile) {
        tft.setTextColor(TFT_BLACK, TFT_RED);
        tft.drawString("Error Opening logfile.", 10, 120, 2);
        Serial.print("Error Opening logfile.");
        flash_error(led_RedSDErr);
      }
    }

    //
    // DO actual work in here:
    //
    if (mb128_boot() != true) {

      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Failed to recognize MB128", 10, 120, 2);

      Serial.println("Failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_read_sectors(0,256, true) == false) {
      
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error while reading from MB128", 10, 120, 2);

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

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Backup completed", 10, 180, 2);
    delay(600);

    tft.fillRect(0, 90, 320, 150, TFT_BLACK);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Executing tests:", 10, 90, 2);


    // then for sector from 0 to 127:
    //   write AA; read sector; compare value -> if bad, then fail
    //   write 55; read sector; compare value -> if bad then fail

    for (sector_num = 0; sector_num < 256; sector_num++)
    {
      sector = sector_num;

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Block #", 10, 120, 4);

      sprintf(char_buf,"%d",sector);
      tft.drawString(char_buf, 100, 120, 4);

      Serial.print("Sector #");
      Serial.print(((sector_num >> 4) & 0x0f), HEX);
      Serial.print((sector_num & 0x0f), HEX);

      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString("0xAA", 10, 150, 4);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("0x55", 80, 150, 4);

      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Write", 155, 150, 4);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Verify", 230, 150, 4);

      Serial.print(" - AA:write");

      if (!mb128_write_sector(sector, buffer_aa)) {
        Serial.println(" - sector failed (1)");
        test_OK = false;
        break;
      }

      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Write", 155, 150, 4);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("Verify", 230, 150, 4);

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
      
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("0xAA", 10, 150, 4);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.drawString("0x55", 80, 150, 4);

      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Write", 155, 150, 4);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Verify", 230, 150, 4);

      Serial.print(" || 55:write");
      if (!mb128_write_sector(sector, buffer_55)) {
        Serial.println(" - sector failed (4)");
        test_OK = false;
        break;
      }

      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("Write", 155, 150, 4);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("Verify", 230, 150, 4);


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
      flash_error(led_RedMB128Err);
    }

    Serial.println("");
    Serial.println("*** Test Successful ***");
    Serial.println("");

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Test completed", 10, 180, 2);
    delay(600);

    tft.fillRect(0, 90, 320, 150, TFT_BLACK);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Restoring:", 10, 90, 2);

    // Now, restore from testtemp
    // Light TEST OK if OK
    Serial.print("Restoring from backup datafile: ");
    Serial.println("testtemp.bkp");
    dataFile = SD.open("testtemp.bkp", FILE_READ);

    if (!dataFile) {
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error Opening datafile.", 10, 120, 2);

      Serial.print("Error Opening datafile.");
      flash_error(led_RedSDErr);
    }
    
// Restore original data

    if (mb128_boot() != true) {

      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Failed to recognize MB128", 10, 120, 2);

      Serial.println("failed to recognize MB128");

      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    }

    if (mb128_write_sectors(0,256, true) == false) {
      
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawString("Error while writing to MB128", 10, 120, 2);
      
      Serial.println("Error while writing to MB128");
      
      dataFile.close();
      if (debug_on) {
        logFile.close();
      }
      flash_error(led_RedMB128Err);
    } 

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Restore Completed", 10, 180, 2);

    Serial.println("Restore completed");

    // If you got this far, everything should be OK
    //
    dataFile.close();
    if (debug_on) {
      logFile.close();
    }

    // if restore() == OK
    delay(600);
}



void loop() {
  int rdval;

  if (digitalRead(sw_BackupPin) == LOW) {
    process_screen();
    backup_button();
    wait_screen();
  }

  if (digitalRead(sw_RestorePin) == LOW) {
    process_screen();
    restore_button();
    wait_screen();
  }

  if (digitalRead(sw_TestPin) == LOW) {
    process_screen();
    test_button();
    wait_screen();
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
