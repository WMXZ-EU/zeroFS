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
extern "C" void loop() {
  // put your main code here, to run repeatedly:
}
