//
#include "TimeLib.h"
#include "zeroFS.h"

#define CREATE_FILES 2

zeroFS_class zFS;
#define NDAT (32*1024)
char data[NDAT];

extern "C" void dprint(char *txt,int val)
{
  Serial.printf(txt,val);
}
void sd_enableChipSelect(void);
void sd_disableChipSelect(void);
void sd_chipSelect(uint16_t high_low); 

int newDay()
{
  static int16_t old=-1;
  int16_t x = day();
  if(x == old) return 0;
  old=x;
  return 1;
}
int newHour()
{
  static int16_t old=-1;
  int16_t x = hour();
  if(x == old) return 0;
  old=x;
  return 1;
}
int newFile()
{
  static int16_t old=-1;
  int16_t x = second()/20;
  if( x == old) return 0;
  old=x;
  return 1;
}

extern "C" void setup() {
  // put your setup code here, to run once:
  while(!Serial);

  Serial.println("Test diskio");
  
  setSyncProvider((getExternalTime) rtc_get);
  Serial.printf("Now: %04d-%02d-%02d_%02d:%02d:%02d\r\n", 
                      year(),month(),day(),hour(),minute(),second());

//  while(!Serial.available());
//  Serial.parseInt();

  Serial.println("Enter 0 to skip write mode or 1 to write test or 2 to write logging");
  while(!Serial.available());
  uint32_t mode = Serial.parseInt();  
  uint32_t flag;
  if(mode>0)
  {
    Serial.println("Enter 0 to start from beginning (create) or 1 to continue logging (append)");
    while(!Serial.available());
    flag = Serial.parseInt();      
  }

  Serial.flush();

  int cs = 10;
  BYTE pdrv = DEV_SPI;
  const char *devName="zero";

  zFS.Init(pdrv, cs, devName);
  Serial.println("Initialized"); //while(1);

  if(mode==1)
  {
    //
    zFS.Create(T_ROOT,flag);  // 0 starts from beginning
    for(int i1=0; i1<5; i1++)
    { char trialname[80];
      sprintf(trialname,"Day_%02d",i1);
      zFS.Create(T_DAY, trialname);
      
      for(int i2=0; i2<5; i2++)
      {  char dayname[80];
        sprintf(dayname,"Hour_%02d%02d",i1,i2);
        zFS.Create(T_HOUR,dayname);
          
        for(int i3=0; i3<5; i3++)
        {  char filename[80];
            sprintf(filename,"File_%02d%02d%02d.dat",i1,i2,i3);
            Serial.print(filename);
            uint32_t t0 = micros();
            zFS.Create(T_FILE,filename);
            uint32_t t1 = micros();
            uint32_t tmn=0xFFFFFFFF,tmx=0;
            for(int ii=0;ii<10;ii++)
            {  for(int nn=0; nn<NDAT; nn++) data[nn]=(ii+nn)%256;
              uint32_t tx = micros();
              zFS.Write((void *)data,NDAT);
              tx=micros()-tx;
              if(tx>tmx) tmx=tx;
              if(tx<tmn) tmn=tx;
            }
            uint32_t t2 = micros();
            zFS.Close(T_FILE);
            uint32_t t3 = micros();
            Serial.printf(" %7d %7d %7d %7d %7d\n",(t1-t0),tmn,tmx,(t3-t2),(t3-t0));
        }
        zFS.Close(T_HOUR);
      }
      zFS.Close(T_DAY);
    }
    zFS.Close(T_ROOT);

    zFS.findEOF();
  
  }
  //
  //
  if(mode==2)
  { 
    static uint16_t open1=0;
    static uint16_t open2=0;
    static uint16_t open3=0;

    zFS.Create(T_ROOT,flag);  //flag = 0 starts from beginning; 1 in append mode
    //
      uint32_t t0=0,t3=0;
      uint32_t tmn=0xFFFFFFFF,tmx=0;
      uint32_t nsect=0;

    while(1)
    { 
      if(newFile())
      {
        if(open3) 
        { 
          zFS.Close(T_FILE);
          t3=micros();
          Serial.printf(" %7d %7d %7d %10d\n",tmn,tmx,(t3-t0),nsect);
        }

        if(newDay())
        { // new day
          if(open1) zFS.Close(T_DAY);
          char dayName[80];
          sprintf(dayName,"Day_%04d%02d%02d",year(),month(),day());
          Serial.println(dayName);
          zFS.Create(T_DAY, dayName);
          open1=1;
        }
        if(newHour())
        { // new day
          if(open2) zFS.Close(T_HOUR);
          char hourName[80];
          sprintf(hourName,"Hour_%02d",hour());
          Serial.println(hourName);
          zFS.Create(T_HOUR,hourName);
          open2=1;
        }
        // new file
        char filename[80];
        sprintf(filename,"File_%02d%02d%02d.dat",hour(),minute(),second());
        Serial.print(filename);
        t0 = micros();
        zFS.Create(T_FILE,filename);
        open3=1;

        tmn=0xFFFFFFFF;
        tmx=0;
        nsect=0;
      }
      
      for(int nn=0; nn<NDAT; nn++) data[nn]=(nn)%256;
      uint32_t tx = micros();
      zFS.Write((void *)data,NDAT);
      tx=micros()-tx;
      if(tx>tmx) tmx=tx;
      if(tx<tmn) tmn=tx;
      nsect += NDAT/1024;
    }
    zFS.Close(T_ROOT);
    zFS.findEOF();
  }

    zFS.findEOF();
    zFS.fixEOF();

    Serial.println("List All");
    zFS.ListAll();
/*
    Serial.println("List T_ROOT");
    zFS.Open(T_ROOT);
    zFS.List();

    Serial.println("List T_DAY");
    zFS.Open(T_DAY);
    zFS.List();

    Serial.println("List T_HOUR");
    zFS.Open(T_HOUR);
    zFS.List();

    Serial.println("List T_HOUR; Day_01, Hour_0101");
    zFS.Open(T_ROOT);
    zFS.Open(T_DAY,"Day_01");
    zFS.Open(T_HOUR,"Hour_0101");
    zFS.List();

    Serial.println("List T_HOUR; Day_03, Hour_0302");
    zFS.Open(T_ROOT);
    zFS.Open(T_DAY,"Day_03");
    zFS.Open(T_HOUR,"Hour_0302");
    zFS.List();

    Serial.println("Read T_FILE; Day_03, Hour_0302, File_030204.dat");
    zFS.Open(T_ROOT);
    zFS.Open(T_DAY,"Day_03");
    zFS.Open(T_HOUR,"Hour_0302");
    zFS.Open(T_FILE,"File_030204.dat");
    zFS.Read((void *)data,NBUF);
    printSector(data);
    Serial.println();
    printSector(data+512);
*/
}

#include "mtp_t4.h"
MTPD    mtpd;

extern "C" void loop() {
  mtpd.loop();
  // put your main code here, to run repeatedly:
}

#define USE_MTP 0
#if USE_MTP==1

#include "SPI.h"
#include "Storage.h"
#include "MTP.h"
MTPStorage_SD storage;
MTPD    mtpd(&storage);

// zeroClasses 
#define USE_ZERO 1
  #ifndef BUILTIN_SCCARD 
    #define BUILTIN_SDCARD 254
  #endif

#if USE_ZERO==1
  // edit SPI to reflect your configuration (following is for T4.1)
  #define SD_MOSI 11
  #define SD_MISO 12
  #define SD_SCK  13

  #define SPI_SPEED SD_SCK_MHZ(33)  // adjust to sd card 

  #if defined (BUILTIN_SDCARD)
    const char *sd_str[]={"sdio","sd1"}; // edit to reflect your configuration
    const int cs[] = {BUILTIN_SDCARD,10}; // edit to reflect your configuration
  #else
    const char *sd_str[]={"sd1"}; // edit to reflect your configuration
    const int cs[] = {10}; // edit to reflect your configuration
  #endif
  const int nsd = sizeof(sd_str)/sizeof(const char *);

zeroFS_class sdx[nsd];
#endif

void storage_configure()
{
  #if USE_ZERO==1
    #if defined SD_SCK
      SPI.setMOSI(SD_MOSI);
      SPI.setMISO(SD_MISO);
      SPI.setSCK(SD_SCK);
    #endif

    for(int ii=0; ii<nsd; ii++)
    { 
      #if defined(BUILTIN_SDCARD)
        if(cs[ii] == BUILTIN_SDCARD)
        {
          if(!sdx[ii].Init(DEV_SDHC)) 
          { Serial.printf("SDIO Storage %d %d %s failed or missing",ii,cs[ii],sd_str[ii]);  Serial.println();
          }
          else
          {
            storage.addFilesystem(sdx[ii], sd_str[ii]);
            uint64_t totalSize = sdx[ii].totalSize();
            uint64_t usedSize  = sdx[ii].usedSize();
            Serial.printf("SDIO Storage %d %d %s ",ii,cs[ii],sd_str[ii]); 
            Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
          }
        }
        else if(cs[ii]<BUILTIN_SDCARD)
      #endif
      {
        pinMode(cs[ii],OUTPUT); digitalWriteFast(cs[ii],HIGH);
        if(!sdx[ii].Init(DEV_SPI))
        { Serial.printf("SD Storage %d %d %s failed or missing",ii,cs[ii],sd_str[ii]);  Serial.println();
        }
        else
        {
          storage.addFilesystem(sdx[ii], sd_str[ii]);
          uint64_t totalSize = sdx[ii].totalSize();
          uint64_t usedSize  = sdx[ii].usedSize();
          Serial.printf("SD Storage %d %d %s ",ii,cs[ii],sd_str[ii]); 
          Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
        }
      }
    }
  #endif
}

void loop() {
  // put your main code here, to run repeatedly:
  mtpd.loop();

  if(Serial.available())
  { //int cmd=Serial.parseInt();
//    mtpd.reset();
  }
}
#endif
