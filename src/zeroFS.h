#ifndef ZEROFS_H
#define ZEROFS_H

#include "core_pins.h"
#include "usb_serial.h"
#include "usb_seremu.h"

#include "diskio.h"

typedef struct
{ uint32_t magic;
  uint32_t type; //HDR1,HDR2,HDR3
  uint32_t sect;
  uint32_t prev;
  uint32_t next;
  uint32_t parent;
  uint32_t size;
  uint32_t ctime;
  uint32_t millis;
  uint32_t micros;
  char name[80];
  uint32_t fill[128 -10 -20];
} HDR_t;

#define MAGIC (0x6F72657A)
typedef enum
{
  T_ROOT=0,
  T_DAY,
  T_HOUR,
  T_FILE,
} HDR_TYPE;


class zeroFS_class
{
    BYTE pdrv;
    uint32_t root_sector;
    uint32_t write_sector;
    uint32_t hdr_sector;
    uint32_t read_sector;
    uint32_t eof_sector;
    uint32_t parent_sector;

    uint32_t total_sectors;
    #define MCOUNT 16
    #define NBUF (MCOUNT*512)
    BYTE buff[NBUF]; // for disk operations
    HDR_t hdr; // for read/write of header
    HDR_t hdr_list[4]={{0}}; // maintain open(last) header
    
  public:
    void Init();
    void Create(HDR_TYPE type, int flag=0);
    void Create(HDR_TYPE type, const char *name);
    void Open(HDR_TYPE type);
    void Open(HDR_TYPE type, const char *name);
    void Close(HDR_TYPE type);
    void Write(void * data, uint32_t ndat);
    void Read(void * data, uint32_t ndat);
    void ListAll(void);
    void ScanAll(void);
    void List(void);
    void findEOF(void);
    void fixEOF(void);
    uint32_t rootSector(void) {return root_sector;}
    uint32_t writeSector(void) {return write_sector;}
    uint32_t readSector(void) {return read_sector;}
    uint32_t eofSector(void) {return eof_sector;}

  /********************for MTP*************/
    uint32_t Count(uint32_t parent);
    uint32_t Next(void);
    uint32_t Info(uint32_t handle, char *filename, uint32_t *size, uint32_t *parent);

    uint32_t GetSize(uint32_t handle) ;
    void Read(uint32_t handle, uint32_t fsize, uint32_t pos, char* out, uint32_t bytes) ;

    uint64_t size()
    {  return (uint64_t)512 *(uint64_t)(total_sectors - root_sector); }
  
    uint64_t free() 
    {  return (uint64_t)512 *(uint64_t)(total_sectors - eof_sector); }
};

void printSector(char *buff);


#endif
