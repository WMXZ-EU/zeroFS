# zeroFS
zero complexity File System

  **Note: ANY existing Filesystem is DESTROYED **
  
## Sequential File System
Data are written sequentially to disk to speed up data logging. To provide some structure three levels of 'directories' are implemented.
- root
- _day1
- __hour1
- ___file1
- ___file2
- ___file3
- __hour2
- ___file4
- ___file5
- ___file6
- _day2
- __hour1
- ___file1
- ___file2

'directories' (root, day, hour) are using one 512 byte sector on disk and keep header information  
## MTP data access
data are acceessed remotely via MTP. The PC accesses the file simiar to a regular file system. MTP implementation is read only to navigate and download the data.

## Development Plans
Implement High-Speed USB (T3.6 and T4.0)
