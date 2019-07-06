#include "zeroFS.h"

zeroFS_class mFS;
#define NBUF (8*1024)
char buff[NBUF];


void setup() {
  // put your setup code here, to run once:
  while(!Serial);
  while(!Serial.available());
  Serial.parseInt();
  Serial.println("Test diskio");
  Serial.println("Enter 0 to start from beginning or 1 to continue logging");
  while(!Serial.available());
  uint32_t flag = Serial.parseInt();  
  uint32_t to=rtc_get();
  Serial.flush();

  mFS.Init();
  //
  mFS.Create(T_ROOT,flag);  // 0 starts from beginning
  for(int i1=0; i1<5; i1++)
  { char trialname[80];
    sprintf(trialname,"Day_%02d",i1);
    mFS.Create(T_DAY, trialname);
    
    for(int i2=0; i2<5; i2++)
    {  char dayname[80];
       sprintf(dayname,"Hour_%02d%02d",i1,i2);
       mFS.Create(T_HOUR,dayname);
        
       for(int i3=0; i3<5; i3++)
       {  char filename[80];
          sprintf(filename,"File_%02d%02d%02d.dat",i1,i2,i3);
          Serial.print(filename);
          uint32_t t0 = micros();
          mFS.Create(T_FILE,filename);
          uint32_t t1 = micros();
          uint32_t tmn=0xFFFFFFFF,tmx=0;
          for(int ii=0;ii<10;ii++)
          {  for(int nn=0; nn<NBUF; nn++) buff[nn]=(ii+nn)%256;
             uint32_t tx = micros();
             mFS.Write((void *)buff,NBUF);
             tx=micros()-tx;
             if(tx>tmx) tmx=tx;
             if(tx<tmn) tmn=tx;
          }
          uint32_t t2 = micros();
          mFS.Close(T_FILE);
          uint32_t t3 = micros();
          Serial.printf(" %7d %7d %7d %7d %7d\n",(t1-t0),tmn,tmx,(t3-t2),(t3-t0));
       }
       mFS.Close(T_HOUR);
    }
     mFS.Close(T_DAY);
  }
  mFS.Close(T_ROOT);
  
  mFS.findEOF();
  
  Serial.println("List All");
  mFS.ListAll();

  Serial.println("List T_ROOT");
  mFS.Open(T_ROOT);
  mFS.List();

  Serial.println("List T_DAY");
  mFS.Open(T_DAY);
  mFS.List();

  Serial.println("List T_HOUR");
  mFS.Open(T_HOUR);
  mFS.List();

  Serial.println("List T_HOUR; Day_01, Hour_0101");
  mFS.Open(T_ROOT);
  mFS.Open(T_DAY,"Day_01");
  mFS.Open(T_HOUR,"Hour_0101");
  mFS.List();

  Serial.println("List T_HOUR; Day_03, Hour_0302");
  mFS.Open(T_ROOT);
  mFS.Open(T_DAY,"Day_03");
  mFS.Open(T_HOUR,"Hour_0302");
  mFS.List();

  Serial.println("Read T_FILE; Day_03, Hour_0302, File_030204.dat");
  mFS.Open(T_ROOT);
  mFS.Open(T_DAY,"Day_03");
  mFS.Open(T_HOUR,"Hour_0302");
  mFS.Open(T_FILE,"File_030204.dat");
  mFS.Read((void *)buff,NBUF);
  printSector(buff);
  Serial.println();
  printSector(buff+512);
}

#define USE_MTP 1
#if USE_MTP==1

#include "mtp.h"
MTPD mtpd;

void loop() {
  // put your main code here, to run repeatedly:
  mtpd.loop();

  if(Serial.available())
  { int cmd=Serial.parseInt();
    mtpd.reset();
  }
}
#else
void loop() {
  // put your main code here, to run repeatedly:
}
#endif
