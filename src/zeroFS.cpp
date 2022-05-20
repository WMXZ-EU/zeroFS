#include "zeroFS.h"
/*
typedef struct partitionTable {
  uint8_t  boot;
  uint8_t  beginHead;
  unsigned beginSector : 6;
  unsigned beginCylinderHigh : 2;
  uint8_t  beginCylinderLow;
  uint8_t  type;
  uint8_t  endHead;
  unsigned endSector : 6;
  unsigned endCylinderHigh : 2;
  uint8_t  endCylinderLow;
  uint32_t firstSector;
  uint32_t totalSectors;
} __attribute__((packed)) part_t;

typedef struct masterBootRecord {
  uint8_t  codeArea[440];
  uint32_t diskSignature;
  uint16_t usuallyZero;
  part_t   part[4];
  uint8_t  mbrSig0;
  uint8_t  mbrSig1;
} __attribute__((packed)) MBR_t;
*/

void printSector(char *buff)
{
  for(int ii=0;ii<512; ii++)
  if((ii+1)%16) Serial.printf("%02x ",buff[ii]); else Serial.printf("%02x\n",buff[ii]);
}

void die(const char * txt, DSTATUS res)
{
  Serial.print(txt); Serial.print(" "); Serial.print(res,HEX); while(1);
}
void die(const char * txt, DRESULT res)
{
  Serial.print(txt); Serial.print(" "); Serial.print(res,HEX); while(1);
}
void die(const char * txt, uint32_t sector, DRESULT res)
{
  Serial.print(txt); Serial.print(" "); 
  Serial.print(sector,HEX); Serial.print(" "); 
  Serial.print(res,HEX); while(1);
}

uint16_t warn(const char * txt, DRESULT res)
{
  if(res)
  {
    Serial.print(txt); Serial.print(" "); Serial.print(res,HEX); return res;
  }
  else
    return 0;
}

#define LastSector (total_sectors -GUARD)

uint32_t sd_cardSize(void) ;

bool zeroFS_class::Init(BYTE device, BYTE cs, const char *str)
{
// DEV_SPI
// DEV_SDHC
// DEV_MSC
// DEV_USB

  pdrv = device;
  strlcpy(devName,str,NameLength);

  csel=cs;
  if(pdrv==DEV_SPI) disk_ioctl(pdrv,1,(void *)&csel); // set cs for spi mode
  
  Serial.print(cs); Serial.print(" "); Serial.println(str);
  
  if(DSTATUS stat = disk_initialize(pdrv)) die("Disk initialize Status: ",stat);

  memset(buff,0,512);
  write_sector=0;
  Write(buff,512);
  //
  root_sector=write_sector;
  total_sectors= sd_cardSize()-root_sector;

  Serial.print("Root Sector: "); Serial.println(root_sector,HEX);
  Serial.print("Sector Count: "); Serial.println(total_sectors);
  return true;
}

void zeroFS_class::findEOF(void)
{
  Serial.println("findEOF");

  read_sector=root_sector;

  DWORD curSector;

  for(int ii=0;ii<4;ii++)
  {
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;

    do
    { 
      curSector=read_sector;
      if(DRESULT res = disk_read (pdrv, buff, read_sector, count)) die("open",res);
      read_sector=hdr.next;
    } while ((read_sector != 0xFFFFFFFFUL) && (hdr.magic==MAGIC));
    read_sector=curSector+1;

    // list last entry
    Serial.printf("%x ",curSector);
    Serial.printf("%x %x %x %x ",hdr.prev,hdr.sect,hdr.next,hdr.parent);
    Serial.printf("%d %x %s\n",hdr.type,hdr.size,hdr.name); Serial.flush();

  }
  eof_sector=read_sector;
}
/*
void zeroFS_class::fixEOF(void)
{
  if(eof_sector==root_sector) return;
  DWORD sector = root_sector;
  BYTE* buff = (BYTE *) &hdr;
  UINT count = 1;
  if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("fixEOF 1 ",res);
  hdr.next = eof_sector;
  if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("fixEOF 2 ",res);
}
*/
void zeroFS_class::Append(void)
{ 
  read_sector=root_sector;

  DWORD curSector;

  for(int ii=0;ii<4;ii++)
  {
    BYTE* buff = (BYTE *) &hdr_list[ii];
    HDR_t *hdr = (HDR_t *) &hdr_list[ii];
    UINT count = 1;
    do
    { 
      curSector=read_sector;
      if(DRESULT res = disk_read (pdrv, buff, read_sector, count)) die("open",res);
      read_sector=hdr->next;
    } while ((read_sector != 0xFFFFFFFFUL) && (hdr->magic==MAGIC));
    read_sector=curSector+1;

    // list last entry
    Serial.printf("%x ",curSector);
    Serial.printf("%x %x %x %x ",hdr->prev,hdr->sect,hdr->next,hdr->parent);
    Serial.printf("%d %x %s\n",hdr->type,hdr->size,hdr->name); Serial.flush();

  }

  Serial.print("Root "); Serial.println(hdr_list[0].sect,HEX);
  Serial.print("Day  "); Serial.println(hdr_list[1].sect,HEX);
  Serial.print("Hour "); Serial.println(hdr_list[2].sect,HEX);
  Serial.print("File "); Serial.println(hdr_list[3].sect,HEX);
  Serial.print("Next "); Serial.println(read_sector,HEX);
}

void zeroFS_class::Create(uint32_t sernum, int flag)
{
  memset(&hdr_list,0,4*sizeof(HDR_t)); // clear all header lists (cache)

  if(flag==0)
  { 
    write_sector = root_sector;
  } 
  else if(flag==1)
  { 
    Append(); //fill existing directory tree
    write_sector=read_sector; 
    //link root to next 
    Close(T_ROOT);
  }

  char name[NameLength];
  sprintf(name,"R%04lx_%08lx",sernum, write_sector);
  Create(T_ROOT, name);
}

void zeroFS_class::Create(HDR_TYPE type, const char *name)
{
  memset(&hdr,0,sizeof(hdr));
  hdr.magic=MAGIC;
  hdr.type=type;
  hdr.prev=write_sector;
  hdr.sect=write_sector;
  hdr.size=1;
  hdr.next=0xFFFFFFFF;
  hdr.ctime=rtc_get();
  hdr.millis=millis();
  hdr.micros=micros();
  strcpy(hdr.name,name);
  
  {    
    if(hdr_list[type].magic==MAGIC) { hdr.prev = hdr_list[type].sect; }
    
    hdr.parent = (type==T_ROOT)? 0xFFFFFFFFUL : hdr_list[type-1].sect;

    // write hdr to disk
    DWORD sector = hdr.sect;
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write1",res);

    memcpy(&hdr_list[type],&hdr, sizeof(HDR_t));

    Serial.print(name); Serial.print(" "); Serial.print(write_sector,HEX); 
    if(type ==T_FILE) Serial.print(" "); else Serial.println();

    write_sector++;
  }
}
void zeroFS_class::Open(HDR_TYPE type)
{ // opens first file of level type
    hdr_sector=root_sector + (type*1);               // for first header
    read_sector=hdr_sector + (type==T_FILE)? 1: 1;   // for data
}
void zeroFS_class::Open(HDR_TYPE type, const char *name)
{ // opens specific file of level type

  //Open(type); // this set hdr_sector
  //int16_t found=0;
  uint32_t sector = hdr_sector;
  while(1)
  { // read hdr from disk
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    
    if(sector==eof_sector)  break;
    
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read1",res);
    if(!strcmp(hdr.name,name)) break;
    sector=hdr.next;
  }
  if(sector==eof_sector) 
  { Serial.print(name); Serial.println(" not found"); while(1);
    return; // end of list
  }
  //
  hdr_sector=hdr.sect;
  read_sector=hdr_sector+1;
}

void zeroFS_class::Close(HDR_TYPE type)
{
  hdr_list[type].next=write_sector;
  //if(type==T_FILE) 
  hdr_list[type].size= (hdr_list[type].next-hdr_list[type].sect);
  //
  DWORD sector = hdr_list[type].sect;
  BYTE* buff = (BYTE *) &hdr_list[type];
  UINT count = 1;      
  if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("close2 ",res);
}

uint32_t zeroFS_class::Write(void * data, uint32_t ndat)
{
    BYTE* buff = (BYTE *) data;
    DWORD sector = write_sector;
    UINT count = (ndat+511)/512;
    if(DRESULT res = disk_write (pdrv, buff, sector, count)) return warn("write",res);

    write_sector += count;
    return count;
}
void zeroFS_class::Read(void * data, uint32_t ndat)
{
      BYTE* buff = (BYTE *) data;
      DWORD sector = read_sector;
      UINT count = (ndat+511)/512;
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read2",res);

      read_sector += count;
}
void zeroFS_class::ListAll(void)
{
  uint32_t sector = root_sector;
  while(1)
  {
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("ListAll ",res);
    //
    if(hdr.magic != MAGIC) break;
    //
    //hdr.size= (hdr.type==T_FILE)? (hdr.next-hdr.sect): 0xfffffffful;
    for(int ii=0; ii<(int)(hdr.type); ii++) Serial.print("  ");
    Serial.printf("%x %x %x %x ",hdr.prev,hdr.sect,hdr.next,hdr.parent);
    Serial.printf("%d %x %s\n",hdr.type,hdr.size,hdr.name); Serial.flush();

    if(hdr.type==T_FILE) 
    { 
      if(hdr.next == 0xFFFFFFFFUL) sector++; else sector = hdr.next;
    }
    else
    { //increase hdr level up to file level
      sector++;
    }
    if(sector==0xFFFFFFFFUL) break;
//    if(sector==eof_sector) break; // end of data reached
  }
}
void zeroFS_class::List(void)
{
  //int16_t done=0;
  
  uint32_t old_type=0;
  uint32_t sector = read_sector;
  while(1)
  {
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);
    if(old_type &&(hdr.type != old_type)) break; // next higher level
    old_type=hdr.type;
    //
    if(hdr.next==0xFFFFFFFF) break;
    for(int ii=0;ii<(int)(hdr.type); ii++) Serial.print("  ");
//    Serial.printf("%x %x %x ",hdr.prev,hdr.sect,hdr.next);
    Serial.printf("%d %d %s\n",hdr.type,hdr.size,hdr.name); Serial.flush();
    sector = hdr.next;
//    if(sector==eof_sector) break; // end of data reached
  }
}

/*********************** for MTP **********************/

uint32_t zeroFS_class::Count(uint32_t parent)
{
  DWORD sector;
  sector = (parent == 0xfffffffful)? root_sector: Index2Sector(parent)+1;
  //
  read_sector = sector;
  uint32_t cnt=0;
    while(1)
    { // search all entries with same parent
      BYTE* buff = (BYTE *) &hdr;
      UINT count = 1;
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("count",sector,res);
      sector = hdr.next;
      if(hdr.magic != MAGIC) break;
      cnt++;      
      if(sector==0xFFFFFFFFUL) break; // end of list
//      if(sector==eof_sector) break; // end of data reached
    }
    return cnt;
}
uint32_t zeroFS_class::Next(void)
{
  //uint32_t old_type=0;
  uint32_t sector = read_sector;
  if(sector==0xFFFFFFFFUL) return 0;
//  if(sector==eof_sector) return 0;

  BYTE* buff = (BYTE *) &hdr;
  UINT count = 1;
  
  if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("next",sector,res);
  
  read_sector = hdr.next;
  uint32_t index = Sector2Index(hdr.sect);
  return index;
}

uint32_t zeroFS_class::Info(uint32_t handle, char *filename, uint32_t *size, uint32_t *parent)
{
  //uint32_t old_type=0;
  uint32_t sector = Index2Sector(handle);
//  if(sector==eof_sector) return 0;

    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read5",res);

    strcpy(filename,hdr.name);
    *size = (hdr.type==T_FILE)? 512*hdr.size : 0xFFFFFFFFUL;
    *parent=Sector2Index(hdr.parent);
    return Sector2Index(hdr.sect);
}

  uint32_t zeroFS_class::GetSize(uint32_t handle) 
  {
    uint32_t sector=Index2Sector(handle);
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read6",res);
    return (hdr.type==T_FILE)? 512*hdr.size : 0xFFFFFFFFUL;
  }

  void zeroFS_class::Read(uint32_t handle, uint32_t fsize, uint32_t pos, char* out, uint32_t bytes) 
  { static uint32_t old_sector=0xfffffffful; // local store for sector number

    // which sector to read from?
    uint32_t sector = Index2Sector(handle)+(pos / NBUF)*1;

    // how many rectors to read?
    UINT count = fsize-pos;
    if(count>NBUF) count=NBUF;
    count /=512;
    if(!count) count=1;
    
    #if DEBUG>0
      Serial.print(pdrv); Serial.print(" ");
      Serial.print(handle); Serial.print(" ");
      Serial.print(sector,HEX); Serial.print(" ");
      Serial.print(old_sector,HEX); Serial.print(" ");
      Serial.print(pos); Serial.print(" ");
      Serial.print(fsize); Serial.print(" ");
      Serial.println(count);
    #endif

    //read sectors if not already done
    if(sector != old_sector)
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read7",res);

    // start point within buffer
    uint32_t spos=pos-(sector-handle)*512;

    // copy data to output
    uint32_t no=spos+bytes;
    if(no < NBUF) // does it fit?
    { 
      memcpy(out,buff+spos,bytes);
    }
    else // fill and get new buffer
    { uint32_t nx1 = NBUF-spos;
      uint32_t nx2 = no-NBUF;
      memcpy(out,buff+spos,nx1);
      sector+=count;
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read8",res);
      memcpy(out+nx1,buff,bytes-nx2);
    }
    // keep sector number for next call
    old_sector=sector;
  }

  uint32_t zeroFS_class::Read(uint32_t sector, char* out, uint32_t count) 
  { if(DRESULT res = disk_read (pdrv, (BYTE*)out, sector, count)) die("read_",res);
    return sector+count;
  }