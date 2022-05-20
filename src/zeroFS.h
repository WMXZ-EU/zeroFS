#ifndef ZEROFS_H
#define ZEROFS_H

#include "core_pins.h"
#include "usb_serial.h"
#include "usb_seremu.h"

#include "diskio.h"

#define NameLength 40
#define MAGIC (0x6F72657A) //"zero"
typedef enum
{
  T_ROOT=0,
  T_DAY,
  T_HOUR,
  T_FILE,
} HDR_TYPE;

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
  char name[NameLength];
  uint32_t fill[128 -10 -(NameLength/4)];
} HDR_t;

//#define Sector2Index(x) (x>>4)
//#define Index2Sector(x) (x<<4)
#define Sector2Index(x) (x)
#define Index2Sector(x) (x)
#define GUARD 100000

class zeroFS_class
{
    uint32_t root_sector;
    uint32_t write_sector;
    uint32_t hdr_sector;
    uint32_t read_sector;
    uint32_t eof_sector;
    uint32_t parent_sector;

    uint32_t total_sectors;

    BYTE pdrv;
    uint16_t csel;
    char devName[NameLength];

    #define MCOUNT 16
    #define NBUF (MCOUNT*512)
    BYTE buff[NBUF]; // for disk operations and header

    HDR_t hdr; // for read/write of header
    HDR_t hdr_list[4]={{0}}; // maintain open(last) header
    
  public:
    bool Init(BYTE device, BYTE cs, const char *name);
    void findEOF(void);
//    void fixEOF(void);
    void Append(void);
    void Create(uint32_t sernum, int flag=0);
    void Create(HDR_TYPE type, const char *name);
    void Open(HDR_TYPE type);
    void Open(HDR_TYPE type, const char *name);
    void Close(HDR_TYPE type);
    uint32_t Write(void * data, uint32_t ndat);
    void Read(void * data, uint32_t ndat);
    void ListAll(void);
    void ScanAll(void);
    void List(void);

    uint32_t totalSectors(void) {return total_sectors;}
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
    uint32_t Read(uint32_t handle, char* out, uint32_t count) ;

    uint64_t size()  {  return (uint64_t)512 *(uint64_t)(total_sectors - root_sector); }
    uint64_t free()  {  return (uint64_t)512 *(uint64_t)(total_sectors - eof_sector); }

    char * getName() {return devName;}

    //uint32_t encode(uint32_t handle);

  /********************FS_Glue*************/
  /*
	File open(const char *filepath, uint8_t mode = FILE_READ) { return 0; }

  bool exists(const char *filepath) { return false; }
	bool mkdir(const char *filepath) { return false; }
	bool rename(const char *oldfilepath, const char *newfilepath) { return false; }
	bool remove(const char *filepath) { return false;	}
	bool rmdir(const char *filepath) { return false;  }
  uint64_t totalSize() { return size(); }
  uint64_t usedSize() { return size() - free(); }
*/
};

void printSector(char *buff);

#endif
