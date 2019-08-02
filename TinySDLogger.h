
/*
TinySDLogger - tiny logger for SD cards. 
This code based on Petit FatFs by ChaN.
*/

#ifndef _TINY_SD_LOGGER_
#define _TINY_SD_LOGGER_

#include <Arduino.h>

// if you are using DS1307 as RTC and want to log time by writeTimestamp call
#define TINY_SD_LOGGER_RTC

// SD PINS (this is software SPI, may use any pins)
#define TINY_SD_LOGGER_CS_PIN   9
#define TINY_SD_LOGGER_MOSI_PIN 8
#define TINY_SD_LOGGER_MISO_PIN 7
#define TINY_SD_LOGGER_SCK_PIN  6

class TinySDLog : public Print 
{
public:

typedef enum 
{
  RC_OK = 0,
  RC_DISK_ERR,
  RC_NOT_READY,
  RC_NOT_ENABLED,
  RC_NO_FILESYSTEM,
  RC_NO_BOOT_RECORD,
  RC_BAD_FAT_TYPE
} ResultCode;

  ResultCode init();
  bool writeTimestamp(void);
  size_t write(uint8_t b);
  ResultCode close();
  
private:
  unsigned char csize;         // Number of sectors per cluster
  unsigned long n_fatent;      // Number of FAT entries (= number of clusters + 2)
  unsigned long fatbase;       // FAT start sector
  unsigned long dirbase;       // Root directory start sector (Cluster# on FAT32)
  unsigned long database;      // Data start sector
  unsigned char numOfFATs;     // Number of FATs
  unsigned long sectorsPerFat; // Number of sectors per FAT 
  unsigned long logFileSize;   // Current size of log file

  unsigned char cardType;
  unsigned int wc; /* Sector write counter */

  // SOFTWARE SPI FUNCTIONS
  void sendSPI(unsigned char d);
  unsigned char receiveSPI(void);
  void initSPI(void); 

  // SD FUNCTIONS
  /* Results of Disk Functions */
  typedef enum 
  {
    RES_OK = 0,   /* 0: Function succeeded */
    RES_ERROR,    /* 1: Disk error */
    RES_NOTRDY,   /* 2: Not ready */
    RES_PARERR    /* 3: Invalid parameter */
  } DRESULT;

  unsigned char initSD(void);
  unsigned char sendSDCommand(unsigned char cmd, unsigned long arg);
  DRESULT readSD(unsigned char *buff, unsigned long sector, unsigned int offset, unsigned int count);
  DRESULT writeSD(const unsigned char *buff, unsigned long sc);
  
  // FAT FUNCTIONS
  unsigned long clust2sect (unsigned long clst);
  ResultCode checkFilesystem(unsigned char *buf, unsigned long sect);
  ResultCode mount();
  ResultCode updateLogFileInfo();
  ResultCode updateSingleFatSector(unsigned char fatNum);
  ResultCode updateFatSector();
  ResultCode initLogFile();
  ResultCode writeLogFile(const void* bufPtr, unsigned int bufSize);
  void print2digits(int number);
};
 
#endif
