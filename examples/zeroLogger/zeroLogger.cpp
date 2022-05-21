//
#include "TimeLib.h"
#include "zeroFS.h"

#define NDAT (4*NBUF)
char data[NDAT];

void sd_enableChipSelect(void);
void sd_disableChipSelect(void);
void sd_chipSelect(uint16_t high_low); 

int newDay(int reset=0)
{
  static int16_t old=-1;
  if(reset) {old=-1; return 1;} 
  int16_t x = day();
  if(x == old) return 0;
  old=x;
  return 1;
}
int newHour(int reset=0)
{
  static int16_t old=-1;
  if(reset) {old=-1; return 1;} 
  int16_t x = hour();
  if(x == old) return 0;
  old=x;
  return 1;
}
int newFile(int reset=0)
{
  static int16_t old=-1;
  if(reset) {old=-1; return 1;} 
  int16_t x = second()/20;
  if( x == old) return 0;
  old=x;
  return 1;
}

uint32_t mode=0;

uint32_t SerNum;

typedef struct
{
  BYTE cs;
  BYTE dev;
  char name[30];
} device_t;

/*
device_t Dev[6] ={{33,DEV_SPI,"sd1"},
                  {34,DEV_SPI,"sd2"},
                  {35,DEV_SPI,"sd3"},
                  {36,DEV_SPI,"sd4"},
                  {37,DEV_SPI,"sd5"},
                  {38,DEV_SPI,"sd6"}};
*/

device_t Dev[1] ={10,DEV_SPI,"zero"};
const uint16_t ndev= sizeof(Dev)/sizeof(device_t);
zeroFS_class zFS[ndev];

// for houskeeping
uint16_t open1=0;
uint16_t open2=0;
uint16_t open3=0;
uint32_t ncount=0;


extern "C" void setup() 
{
  // put your setup code here, to run once:
  while(!Serial);

  Serial.println("Test zeroLogger");

#if defined(__IMXRT1062__)
  SerNum = HW_OCOTP_MAC0 & 0xFFFFFF;
  Serial.print("Serial Number "); Serial.println((int32_t)SerNum,HEX);
#else
  SerNum = 0xFFFFFF;
#endif  
  setSyncProvider((getExternalTime) rtc_get);
  Serial.printf("Now: %04d-%02d-%02d_%02d:%02d:%02d\r\n", 
                      year(),month(),day(),hour(),minute(),second());

  Serial.println("Enter 0 to skip logging or 1 to perform logging");
  while(!Serial.available());
  mode = Serial.parseInt();  

  uint32_t flag=1;
  if(mode>0)
  {
    Serial.println("Enter 0 to reset to beginning, 1 to continue logging");
    while(!Serial.available());
    flag = Serial.parseInt();      
  }

  Serial.flush();

  zFS[0].Init(Dev[0].dev, Dev[0].cs, Dev[0].name);
  Serial.println("Initialized"); //while(1);
  delay(100);
/*
  disk_read (Dev[0].dev, (BYTE *) data, 0, 1);
  printSector(data);
*/

  if(mode)
  {
    Serial.println("Do Logging");
    zFS[0].Create(SerNum,flag);
    if(flag==1)
    { // force closing 
      open1=0;
      open2=0;
      open3=0;
    }
  }
  else
  {
    Serial.println("List All");
    zFS[0].ListAll();
  }
}

#include "mtp_device.h"
MTPD    mtpd;

// for statistics
uint32_t t0=0,t3=0;
uint32_t tmn=0xFFFFFFFF,tmx=0;

  #define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
  #define CPU_RESTART_VAL 0x5FA0004
  #define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL)

extern "C" void loop() 
{ int32_t cmd;
  if(Serial.available())
  {
    cmd = Serial.parseInt();      
    if(cmd==0)
    {
      CPU_RESTART;
    }
  }
  if(mode==0) // we are only addressing MTP
  { // access MTP
    mtpd.loop();
    return;
  }
  else
  { 
    // logging
    uint32_t nbuf=NDAT;
    if(newFile())
    { 
      if(open3) 
      { // have file open, so close it
        zFS[0].Close(T_FILE);
        t3=micros();
        Serial.printf(" %7d %7d %7d %7d %f\n",tmn,tmx,(t3-t0),ncount,((float)ncount*nbuf)/(float)(t3-t0));
        // reset statistics
        tmn=0xFFFFFFFF;
        tmx=0;
      }

      // new day directory
      if(newDay())
      { // new day
        if(open1) zFS[0].Close(T_DAY); // close directory if required
        // create new directory
        char dayName[40];
        sprintf(dayName,"Day_%04d%02d%02d",year(),month(),day());
        zFS[0].Create(T_DAY, dayName);
        open1=1;
      }

      // new hour directory
      if(newHour())
      { // new day
        if(open2) zFS[0].Close(T_HOUR); // close directory if required
        // create new directory
        char hourName[40];
        sprintf(hourName,"Hour_%02d",hour());
        zFS[0].Create(T_HOUR,hourName);
        open2=1;
      }

      // create new file
      char filename[40];
      sprintf(filename,"File_%02d%02d%02d.dat",hour(),minute(),second());
      t0 = micros();
      zFS[0].Create(T_FILE,filename);
      open3=1;
//      nbuf=NDAT-512; //first record is smaller (really?)
      ncount=0;

    }

    // fetch and store data
    for(uint32_t nn=0; nn<nbuf; nn++) data[nn]=(nn)%256;
    //
    uint32_t tx = micros();
    zFS[0].Write((void *)data,nbuf);
    // update statistics
    tx=micros()-tx;
    if(tx>tmx) tmx=tx;
    if(tx<tmn) tmn=tx;
    ncount++;
  }
}
