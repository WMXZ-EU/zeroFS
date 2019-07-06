#include "zeroFS.h"

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

void printSector(char *buff)
{
  for(int ii=0;ii<512; ii++)
  if((ii+1)%16) Serial.printf("%02x ",buff[ii]); else Serial.printf("%02x\n",buff[ii]);
}

void die(const char * txt, DRESULT res)
{
  Serial.print(txt); Serial.print(" "); Serial.print(res); while(1);
}

void zeroFS_class::Init()
{
// DEV_SPI
// DEV_SDHC
// DEV_MSC
// DEV_USB
  pdrv = DEV_SDHC;

  MBR_t mbr;

  if(DSTATUS stat = disk_initialize(pdrv))
  { Serial.print("Disk initialize Status: "); Serial.println(stat);  while(1);}

  BYTE* buff = (BYTE *) &mbr;
  DWORD sector = 0;
  UINT count = 1;
  
  if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("init",res);
  
  Serial.println("\nMaster Boot Record");
  printSector((char *) buff);
  { int ii=0;
    Serial.print("Signature: "); Serial.println(mbr.diskSignature,HEX);
    Serial.print("  Partition: "); Serial.print(ii);
    Serial.print(" first Sector: ");
    Serial.print(mbr.part[ii].firstSector,HEX);
    Serial.print(" total Sectors: ");
    Serial.println(mbr.part[ii].totalSectors,HEX);
    
    Serial.println(mbr.part[ii].boot,HEX);
    Serial.printf("%02x ",mbr.part[ii].beginHead);
    Serial.printf("%02x ",mbr.part[ii].beginSector);
    Serial.printf("%02x ",mbr.part[ii].beginCylinderHigh);
    Serial.printf("%02x\n",mbr.part[ii].beginCylinderLow);
    
    Serial.println(mbr.part[ii].type,HEX);
    Serial.printf("%02x ",mbr.part[ii].endHead);
    Serial.printf("%02x ",mbr.part[ii].endSector);
    Serial.printf("%02x ",mbr.part[ii].endCylinderHigh);
    Serial.printf("%02x\n",mbr.part[ii].endCylinderLow);
  }  
  Serial.println(mbr.mbrSig0,HEX); //0x55
  Serial.println(mbr.mbrSig1,HEX); //0xAA
  
  root_sector=mbr.part[0].firstSector;
  total_sectors=mbr.part[0].totalSectors;

  // clean up earlier disk usage
  findEOF();
  fixEOF();
}

void zeroFS_class::Create(HDR_TYPE type, int flag)
{
  if(type==T_ROOT)
  {
    memset(&hdr_list,0,4*sizeof(HDR_t));
    if(flag==0) // initialize start to root sector
    { 
      write_sector = root_sector;
      eof_sector=root_sector;
      hdr_list[type].prev=write_sector;
    }
    else        // continue at end of previous session
    {   uint32_t sector = root_sector;
        BYTE* buff = (BYTE *) &hdr;
        UINT count = 1;

        while(1)
        {
          if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read3",res);
          if(hdr.magic!=MAGIC) break;       // gone over eof_sector
          if(hdr.next==0xFFFFFFFFul) break; // reached last unclosed header
          
          sector = write_sector = eof_sector = hdr.next; 
          hdr_list[type].prev=hdr.sect;
        }
        if((sector==root_sector) && (hdr.magic != MAGIC))
        { // fall back to root_sector
            write_sector = eof_sector = root_sector;
            hdr_list[type].prev=write_sector;
        }
    }
    char name[80];
    sprintf(name,"R_0X%08x",write_sector);
    Create(type, name);
  }
  Serial.println(write_sector,HEX);
}
void zeroFS_class::Create(HDR_TYPE type, const char *name)
{
  //
  memset(&hdr,0,sizeof(hdr));
  hdr.magic=MAGIC;
  hdr.type=type;
  hdr.prev=write_sector;
  hdr.sect=write_sector;
  hdr.next=0xFFFFFFFF;
  hdr.ctime=rtc_get();
  hdr.millis=millis();
  hdr.micros=micros();
  strcpy(hdr.name,name);
  
  {    
    if(hdr_list[type].magic==MAGIC)
    {  hdr.prev = hdr_list[type].sect;
    }
    
    if(type==T_ROOT)
    {
      hdr.parent = 0xfffffffful;
    }
    else
    {
      hdr.parent = hdr_list[type-1].sect;
    }

    // write hdr to disk
    BYTE* buff = (BYTE *) &hdr;
    DWORD sector = hdr.sect;
    UINT count = 1;
    
    if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write1",res);

    memcpy(&hdr_list[type],&hdr, sizeof(hdr));
    write_sector=write_sector+1;
  }
}
void zeroFS_class::Open(HDR_TYPE type)
{ // opens first file of level type
    hdr_sector=root_sector+(type);
    read_sector=hdr_sector+1;
}
void zeroFS_class::Open(HDR_TYPE type, const char *name)
{ // opens specific file of level type

  //Open(type); // this set hdr_sector
  int16_t found=0;
  uint32_t sector = hdr_sector+1;
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
  if(type==T_ROOT)
  {
    memset(&hdr,0,sizeof(hdr));
    BYTE* buff = (BYTE *) &hdr;
    DWORD sector = write_sector;
    UINT count = 1;
      
    if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write2",res);

  }
  
  hdr_list[type].next=write_sector;
  //
  BYTE* buff = (BYTE *) &hdr_list[type];
  DWORD sector = hdr_list[type].sect;
  UINT count = 1;
      
  if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write2",res);
  
}
void zeroFS_class::Write(void * data, uint32_t ndat)
{
      BYTE* buff = (BYTE *) data;
      DWORD sector = write_sector;
      UINT count = ndat/512;
      
      if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write3",res);

      write_sector += count;
  
}
void zeroFS_class::Read(void * data, uint32_t ndat)
{
      BYTE* buff = (BYTE *) data;
      DWORD sector = read_sector;
      UINT count = ndat/512;
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
    
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read3",res);
    //
    if(hdr.type==1) Serial.println(); 
    for(int ii=0;ii<(hdr.type); ii++) Serial.print("  ");
    Serial.printf("%x %x %x %x ",hdr.prev,hdr.sect,hdr.next,hdr.parent);
    Serial.printf("%d %d %s\n",hdr.type,hdr.next-hdr.sect,hdr.name); Serial.flush();
    if(hdr.type<3) 
    {
      sector++;
    }
    else
    { if(hdr.next==0xFFFFFFFF) break;
      sector = hdr.next;
      if(sector==eof_sector) break; // end of data reached
    }
  }
}
void zeroFS_class::List(void)
{
  int16_t done=0;
  
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
    for(int ii=0;ii<(hdr.type); ii++) Serial.print("  ");
//    Serial.printf("%x %x %x ",hdr.prev,hdr.sect,hdr.next);
    Serial.printf("%d %d %s\n",hdr.type,hdr.next-hdr.sect,hdr.name); Serial.flush();
    sector = hdr.next;
    if(sector==eof_sector) break; // end of data reached
  }
}

void zeroFS_class::findEOF(void)
{
  int16_t done=0;
  
  uint32_t old_type=0;
  uint32_t sector = root_sector;
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;

    // root level
    eof_sector=sector;
    while(1) // should only be 1 root header, but ....
    {   if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read5",res);
        if(hdr.magic != MAGIC) break; 
        if(hdr.next==0xFFFFFFFF) break; 
        sector=hdr.next;
        eof_sector=sector;
    }
    if(hdr.type != T_ROOT)
    { //Serial.print("EOF0 "); Serial.println(eof_sector,HEX);
      return;
    }

    if(hdr.magic != MAGIC) // reached end of list
    { //Serial.print("EOF1 "); Serial.println(eof_sector,HEX);
      return;
    }

    // go to trial level
    sector++;
    eof_sector=sector;
    while(1)
    {
        if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read6",res);
        if(hdr.magic != MAGIC) break; 
        if(hdr.next==0xFFFFFFFF) break; 
        sector=hdr.next;
        eof_sector=sector;
    }
    if(hdr.magic != MAGIC) // reached end of list
    { //Serial.print("EOF2 "); Serial.println(eof_sector,HEX);
      return;
    }

    // go to day level
    sector++;
    eof_sector=sector;
    while(1)
    {
        if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read7",res);
        if(hdr.magic != MAGIC) break; 
        if(hdr.next==0xFFFFFFFF) break; 
        sector=hdr.next;
        eof_sector=sector;
    }
    if(hdr.magic != MAGIC) // reached end of list
    {// Serial.print("EOF3 "); Serial.println(eof_sector,HEX);
      return;
    }
    // go to file level
    sector++;
    eof_sector=sector;
    while(1)
    {
        if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read7",res);
        if(hdr.magic != MAGIC) break; 
        if(hdr.next==0xFFFFFFFF) break; 
        sector=hdr.next;
        eof_sector=sector;
    }
    if(hdr.magic != MAGIC) // reached end of list
    {// Serial.print("EOF3 "); Serial.println(eof_sector,HEX);
      return;
    }
    //Serial.print("EOF4 "); Serial.println(eof_sector,HEX);
}

void zeroFS_class::fixEOF(void)
{
  int16_t done=0;
  
    if(eof_sector==root_sector) return;

    // root level;
  uint32_t old_type=0;
  uint32_t sector = root_sector;
    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    
    while(1)
    {
        if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read8",res);
        if((hdr.next== eof_sector) || (hdr.next==0xFFFFFFFF)) break; 
        sector=hdr.next;
    }
    if(hdr.next == 0xFFFFFFFF) 
    { hdr.next=eof_sector;
      BYTE* buff = (BYTE *) &hdr;
      DWORD sector = hdr.sect;
      UINT count = 1;
        
      if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write4",res);
    }
    else
     return;

    // go to day level
    sector++;
    while(1)
    {
        if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read9",res);
        if((hdr.next==eof_sector) || (hdr.next==0xFFFFFFFF)) break; 
        sector=hdr.next;
    }
    if(hdr.next == 0xFFFFFFFF) 
    { hdr.next=eof_sector;
      BYTE* buff = (BYTE *) &hdr;
      DWORD sector = hdr.sect;
      UINT count = 1;
        
      if(DRESULT res = disk_write (pdrv, buff, sector, count)) die("write5",res);
    }
}


/* for MTP */

uint32_t zeroFS_class::Count(uint32_t parent)
{
  parent_sector=parent;
  if(parent==0xfffffffful)
  { uint32_t sector = root_sector;
    uint32_t cnt=0;
    while(1)
    {
      BYTE* buff = (BYTE *) &hdr;
      UINT count = 1;
      
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read3",res);
      cnt++;      
      sector = hdr.next;
      if(sector==eof_sector) break; // end of data reached
    }
    read_sector=root_sector;
    return cnt;
  }
  else
  {
    uint32_t sector = parent+1;
    uint32_t cnt=0;
    HDR_TYPE type=0;
    while(1)
    {
      BYTE* buff = (BYTE *) &hdr;
      UINT count = 1;
      
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read3",res);
      if(type==0) 
        type=hdr.type;
      else if(hdr.type != type) 
        break; 
      cnt++;
      sector = hdr.next;
      if(sector==eof_sector) break; // end of data reached
    }
    read_sector=parent+1;
    return cnt;
  }
}
uint32_t zeroFS_class::Next(void)
{
  uint32_t old_type=0;
  uint32_t sector = read_sector;
  HDR_TYPE type=0;

  if(sector==eof_sector) return 0;

    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;
    
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);
    if(type==0) 
      type=hdr.type;
    else if(hdr.type != type) 
      return 0; 

    read_sector=hdr.next;
    return hdr.sect;
}

uint32_t zeroFS_class::Info(uint32_t handle, char *filename, uint32_t *size, uint32_t *parent)
{
  uint32_t old_type=0;
  uint32_t sector = handle;
  if(sector==eof_sector) return 0;

    BYTE* buff = (BYTE *) &hdr;
    UINT count = 1;

    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);

    strcpy(filename,hdr.name);
    if(hdr.type==T_FILE)
    {
      *size=512*(hdr.next-hdr.sect);
    }
    else
    {
      *size=0xFFFFFFFFul;
    }
    *parent=hdr.parent;
    return hdr.sect;
}

  uint32_t zeroFS_class::GetSize(uint32_t handle) 
  {
    BYTE* buff = (BYTE *) &hdr;
    uint32_t sector=handle;
    UINT count = 1;
    if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);

    return 512*(hdr.next-hdr.sect);
  }

  void zeroFS_class::Read(uint32_t handle, uint32_t fsize, uint32_t pos, char* out, uint32_t bytes) 
  { static uint32_t old_sector=0; // local store for sector number

    // which sector to read from?
    uint32_t sector = handle+(pos / NBUF)*MCOUNT;

    // how much rectors to read?
    UINT count = fsize-pos;
    if(count>NBUF) count=NBUF;
    count /=512;
    if(!count) count=1;
    
    //read sectors if not already done
    if(sector != old_sector)
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);

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
      if(DRESULT res = disk_read (pdrv, buff, sector, count)) die("read4",res);
      memcpy(out+nx1,buff,bytes-nx2);
    }
    // keep sector number for next call
    old_sector=sector;
  }
