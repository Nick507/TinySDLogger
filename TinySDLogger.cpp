
#include "TinySDLogger.h"

#ifdef TINY_SD_LOGGER_RTC
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#endif

/***************************************************************************
****************************************************************************
                  S O F T W A R E   S P I   F O R   S D
****************************************************************************
****************************************************************************/

#define SELECT() digitalWrite(TINY_SD_LOGGER_CS_PIN, LOW)
#define DESELECT() digitalWrite(TINY_SD_LOGGER_CS_PIN, HIGH)
#define SELECTING !digitalRead(TINY_SD_LOGGER_CS_PIN)

void TinySDLog::sendSPI(unsigned char d) 
{
  shiftOut(TINY_SD_LOGGER_MOSI_PIN, TINY_SD_LOGGER_SCK_PIN, MSBFIRST, d);
}

unsigned char TinySDLog::receiveSPI(void) 
{
  digitalWrite(TINY_SD_LOGGER_MOSI_PIN, HIGH);
  return shiftIn(TINY_SD_LOGGER_MISO_PIN, TINY_SD_LOGGER_SCK_PIN, MSBFIRST);
}

void TinySDLog::initSPI(void) 
{
  pinMode(TINY_SD_LOGGER_SCK_PIN, OUTPUT);
  pinMode(TINY_SD_LOGGER_MOSI_PIN, OUTPUT);
  pinMode(TINY_SD_LOGGER_MISO_PIN, INPUT);
  DESELECT();
  pinMode(TINY_SD_LOGGER_CS_PIN, OUTPUT);
}

/***************************************************************************
****************************************************************************
                                 S D
****************************************************************************
****************************************************************************/

/* Definitions for MMC/SDC command */
#define CMD0   (0x40+0)  /* GO_IDLE_STATE */
#define CMD1   (0x40+1)  /* SEND_OP_COND (MMC) */
#define ACMD41 (0xC0+41) /* SEND_OP_COND (SDC) */
#define CMD8   (0x40+8)  /* SEND_IF_COND */
#define CMD16  (0x40+16) /* SET_BLOCKLEN */
#define CMD17  (0x40+17) /* READ_SINGLE_BLOCK */
#define CMD24  (0x40+24) /* WRITE_BLOCK */
#define CMD55  (0x40+55) /* APP_CMD */
#define CMD58  (0x40+58) /* READ_OCR */

#define STA_NOINIT    0x01  /* Drive not initialized */

/* Card type flags (cardType) */
#define CT_MMC        0x01  /* MMC ver 3 */
#define CT_SD1        0x02  /* SD ver 1 */
#define CT_SD2        0x04  /* SD ver 2 */
#define CT_SDC        (CT_SD1|CT_SD2) /* SD */
#define CT_BLOCK      0x08  /* Block addressing */

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

unsigned char TinySDLog::sendSDCommand(unsigned char cmd, unsigned long arg)
{
  unsigned char n, res;

  if (cmd & 0x80) 
  { /* ACMD<n> is the command sequense of CMD55-CMD<n> */
    cmd &= 0x7F;
    res = sendSDCommand(CMD55, 0);
    if (res > 1) return res;
  }

  /* Select the card */
  DESELECT();
  receiveSPI();
  SELECT();
  receiveSPI();

  /* Send a command packet */
  sendSPI(cmd);            /* Start + Command index */
  sendSPI((unsigned char)(arg >> 24));    /* Argument[31..24] */
  sendSPI((unsigned char)(arg >> 16));    /* Argument[23..16] */
  sendSPI((unsigned char)(arg >> 8));     /* Argument[15..8] */
  sendSPI((unsigned char)arg);        /* Argument[7..0] */
  n = 0x01;             /* Dummy CRC + Stop */
  if (cmd == CMD0) n = 0x95;      /* Valid CRC for CMD0(0) */
  if (cmd == CMD8) n = 0x87;      /* Valid CRC for CMD8(0x1AA) */
  sendSPI(n);

  /* Receive a command response */
  n = 10;               /* Wait for a valid response in timeout of 10 attempts */
  do 
  {
    res = receiveSPI();
  } while ((res & 0x80) && --n);

  return res;     /* Return with the response value */
}

/*-----------------------------------------------------------------------*/
/* Delay                                                 */
/*-----------------------------------------------------------------------*/

static void dly_100us (void) {
  // each count delays four CPU cycles.
  _delay_loop_2(F_CPU/(40000));
}

/*-----------------------------------------------------------------------*/
/* Read partial sector                                                   */
/*-----------------------------------------------------------------------*/

/* Status of Disk Functions */

TinySDLog::DRESULT TinySDLog::readSD(
  unsigned char *buff,   /* Pointer to the read buffer (NULL:Forward to the stream) */
  unsigned long sector, /* Sector number (LBA) */
  unsigned int offset,  /* Byte offset to read from (0..511) */
  unsigned int count    /* Number of bytes to read (ofs + cnt mus be <= 512) */
)
{
  DRESULT res = RES_ERROR;
  unsigned char rc;
  unsigned int bc;
  
  if (!(cardType & CT_BLOCK)) sector *= 512;  /* Convert to byte address if needed */

  if (sendSDCommand(CMD17, sector) == 0) { /* READ_SINGLE_BLOCK */

    bc = 40000; /* Time counter */
    do {        /* Wait for data packet */
      rc = receiveSPI();
    } while (rc == 0xFF && --bc);

    if (rc == 0xFE) { /* A data packet arrived */

      bc = 512 + 2 - offset - count;  /* Number of trailing bytes to skip */

      /* Skip leading bytes */
      while (offset--) receiveSPI();

      /* Receive a part of the sector */
      if (buff) { /* Store data to the memory */
        do {
          *buff++ = receiveSPI();
        } while (--count);
      } else {  /* Forward data to the outgoing stream */
        do {
          receiveSPI();
        } while (--count);
      }

      /* Skip trailing bytes and CRC */
      do receiveSPI(); while (--bc);

      res = RES_OK;
    }
  }

  DESELECT();
  receiveSPI();

  return res;
}

/*-----------------------------------------------------------------------*/
/* Write partial sector                                                  */
/*-----------------------------------------------------------------------*/

TinySDLog::DRESULT TinySDLog::writeSD (
  const unsigned char *buff, /* Pointer to the bytes to be written (NULL:Initiate/Finalize sector write) */
  unsigned long sc      /* Number of bytes to send, Sector number (LBA) or zero */
)
{
  unsigned int bc;

  if (buff) 
  {   /* Send data bytes */
    bc = sc;
    while (bc && wc) 
    {   /* Send data bytes to the card */
      sendSPI(*buff++);
      wc--; 
      bc--;
    }
    return RES_OK;
  } 

  if (sc) 
  { /* Initiate sector write process */
    if (!(cardType & CT_BLOCK)) sc *= 512;  /* Convert to byte address if needed */
    if (sendSDCommand(CMD24, sc)) return RES_ERROR; 
    /* WRITE_SINGLE_BLOCK */
    sendSPI(0xFF); 
    sendSPI(0xFE);   /* Data block header */
    wc = 512;             /* Set byte counter */
    return RES_OK;
  } 

  /* Finalize sector write process */
  DRESULT res = RES_ERROR;
  bc = wc + 2;
  while (bc--) sendSPI(0); /* Fill left bytes and CRC with zeros */
  if((receiveSPI() & 0x1F) == 0x05) 
  { /* Receive data resp and wait for end of write process in timeout of 500ms */
    for (bc = 5000; receiveSPI() != 0xFF && bc; bc--) dly_100us(); /* Wait for ready */
    if(bc) res = RES_OK;
    DESELECT();
    receiveSPI();
  }
  return res;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

unsigned char TinySDLog::initSD(void)
{
  unsigned char n, cmd, ty, ocr[4];
  unsigned int tmr;

  if (cardType && SELECTING) writeSD(0, 0); /* Finalize write process if it is in progress */

  initSPI();   /* Initialize ports to control MMC */
  DESELECT();
  for (n = 10; n; n--) receiveSPI(); /* 80 dummy clocks with CS=H */

  ty = 0;
  if (sendSDCommand(CMD0, 0) == 1) {     /* GO_IDLE_STATE */
    if (sendSDCommand(CMD8, 0x1AA) == 1) { /* SDv2 */
      for (n = 0; n < 4; n++) ocr[n] = receiveSPI();   /* Get trailing return value of R7 resp */
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {     /* The card can work at vdd range of 2.7-3.6V */
        for (tmr = 10000; tmr && sendSDCommand(ACMD41, 1UL << 30); tmr--) dly_100us(); /* Wait for leaving idle state (ACMD41 with HCS bit) */
        if (tmr && sendSDCommand(CMD58, 0) == 0) {   /* Check CCS bit in the OCR */
          for (n = 0; n < 4; n++) ocr[n] = receiveSPI();
          ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;  /* SDv2 (HC or SC) */
        }
      }
    } else {              /* SDv1 or MMCv3 */
      if (sendSDCommand(ACMD41, 0) <= 1)   {
        ty = CT_SD1; cmd = ACMD41;  /* SDv1 */
      } else {
        ty = CT_MMC; cmd = CMD1;  /* MMCv3 */
      }
      for (tmr = 10000; tmr && sendSDCommand(cmd, 0); tmr--) dly_100us();  /* Wait for leaving idle state */
      if (!tmr || sendSDCommand(CMD16, 512) != 0)      /* Set R/W block length to 512 */
        ty = 0;
    }
  }
  cardType = ty;
  DESELECT();
  receiveSPI();

  return ty ? 0 : STA_NOINIT;
}
/***************************************************************************
****************************************************************************
                                 F A T
****************************************************************************
****************************************************************************/

#define BPB_SecPerClus  13
#define BPB_RsvdSecCnt  14
#define BPB_NumFATs     16
#define BPB_RootEntCnt  17
#define BPB_TotSec16    19
#define BPB_FATSz16     22
#define BPB_TotSec32    32
#define BS_55AA         510
#define BPB_FATSz32     36
#define BPB_RootClus    44
#define BS_FilSysType32 82

#define MBR_Table       446

#define DIR_FileSize    28

/*--------------------------------*/
/* Multi-byte word access macros  */

#define LD_WORD(ptr)    (unsigned short)(*(unsigned short*)(unsigned char*)(ptr))
#define LD_DWORD(ptr)   (unsigned long)(*(unsigned long*)(unsigned char*)(ptr))
#define ST_WORD(ptr,val)  *(unsigned short*)(unsigned char*)(ptr)=(unsigned short)(val)
#define ST_DWORD(ptr,val) *(unsigned long*)(unsigned char*)(ptr)=(unsigned long)(val)

/*-----------------------------------------------------------------------*/
/* Get sector# from cluster# / Get cluster field from directory entry    */
/*-----------------------------------------------------------------------*/

unsigned long TinySDLog::clust2sect (unsigned long clst)
{
	clst -= 2;
	if (clst >= (n_fatent - 2)) return 0;		/* Invalid cluster# */
	return (unsigned long)clst * csize + database;
}

/*-----------------------------------------------------------------------*/
/* Check a sector if it is an FAT boot record                            */
/*-----------------------------------------------------------------------*/

TinySDLog::ResultCode TinySDLog::checkFilesystem(unsigned char *buf, unsigned long sect)
{
  /* Read the boot record */
	if (readSD(buf, sect, 510, 2)) return RC_DISK_ERR;		

  /* Check record signature */
	if (LD_WORD(buf) != 0xAA55) return RC_NO_BOOT_RECORD;				
		
  /* Check FAT32 */
	if (!readSD(buf, sect, BS_FilSysType32, 2) && LD_WORD(buf) == 0x4146) return RC_OK;	
		
	return RC_BAD_FAT_TYPE;
}

/*-----------------------------------------------------------------------*/
/* Mount/Unmount a Locical Drive                                         */
/*-----------------------------------------------------------------------*/

TinySDLog::ResultCode TinySDLog::mount()
{
	unsigned char buf[36];
	unsigned long bsect = 0, fsize, tsect, mclst;

	if (initSD() & STA_NOINIT) return RC_NOT_READY;	/* Check if the drive is ready or not */

	/* Search FAT partition on the drive */
	ResultCode res = checkFilesystem(buf, bsect);			/* Check sector 0 as an SFD format */
	if (res == RC_NO_BOOT_RECORD) 
	{
	  /* Not an FAT boot record, it may be FDISK format */
		/* Check a partition listed in top of the partition table */
		if (readSD(buf, bsect, MBR_Table, 16)) return RC_DISK_ERR;
		if (buf[4]) 
		{					/* Is the partition existing? */
			bsect = LD_DWORD(&buf[8]);	/* Partition offset in LBA */
			res = checkFilesystem(buf, bsect);	/* Check the partition */
		}
	} 
	if(res) return res;

	/* Initialize the file system object */
	if (readSD(buf, bsect, 13, sizeof (buf))) return RC_DISK_ERR;

	fsize = LD_WORD(buf+BPB_FATSz16-13);				/* Number of sectors per FAT */
	if (!fsize) fsize = LD_DWORD(buf+BPB_FATSz32-13);
  sectorsPerFat = fsize;

  numOfFATs = buf[BPB_NumFATs-13];
	fsize *= numOfFATs;						/* Number of sectors in FAT area */
	fatbase = bsect + LD_WORD(buf+BPB_RsvdSecCnt-13); /* FAT start sector (lba) */
	csize = buf[BPB_SecPerClus-13];					/* Number of sectors per cluster */
	unsigned short n_rootdir = LD_WORD(buf+BPB_RootEntCnt-13);		/* Nmuber of root directory entries */
	tsect = LD_WORD(buf+BPB_TotSec16-13);				/* Number of sectors on the file system */
	if (!tsect) tsect = LD_DWORD(buf+BPB_TotSec32-13);
	mclst = (tsect						/* Last cluster# + 1 */
		- LD_WORD(buf+BPB_RsvdSecCnt-13) - fsize - n_rootdir / 16
		) / csize + 2;
	n_fatent = (unsigned long)mclst;

  // validate that filesystem is FAT32
	if (mclst < 0xFFF7) return RC_NO_FILESYSTEM;

	dirbase = LD_DWORD(buf+(BPB_RootClus-13));	/* Root directory start cluster */
	database = fatbase + fsize + n_rootdir / 16;	/* Data start sector (lba) */

	return RC_OK;
}

// ===========================================================================

const unsigned char logFileFirstCluster = 128; // 512/4 - one sector shift from 
const unsigned char logFileInfoSector = 1;

static unsigned char logFileInfo[32] = 
  {0x4C, 0x4F, 0x47, 0x20, 0x20, 0x20, 0x20, 0x20, // file name LOG.TXT
   0x54, 0x58, 0x54, // file extension
   0x20,  // attributes
   0x00,  // reserved
   0xBC,  // created time refeinment in 10ms
   0x47, 0xAD, 0xF4, 0x4E,  // created date/time
   0xF4, 0x4E, //last access date
   0x00, 0x00,  //first cluster (high word)
   0x48, 0xAD, 0xF4, 0x4E, // modified date/time
   logFileFirstCluster, 0x00, // first cluster (low word)
   0x00, 0x00, 0x00, 0x00 // file size
   };

TinySDLog::ResultCode TinySDLog::updateLogFileInfo ()
{
  // prepare sector for writing
  if(writeSD(0, clust2sect(dirbase) + logFileInfoSector)) return RC_DISK_ERR;

  ST_DWORD(logFileInfo + DIR_FileSize, logFileSize);
  
  // write file info
  if (writeSD(logFileInfo, sizeof(logFileInfo))) return RC_DISK_ERR;

  // finalize sector writing
  if (writeSD(0, 0)) return RC_DISK_ERR;

  return RC_OK;
}

TinySDLog::ResultCode TinySDLog::updateSingleFatSector(unsigned char fatNum)
{
  unsigned long cluster;
  unsigned long fatRec;

  cluster = logFileFirstCluster + (logFileSize + 1) / (512 * csize);

  if(((cluster & 0x7F) == 0) && cluster)
  {
    // need to remove EOC from previous sector
    // prepare sector for writing
    unsigned long prevCluster = cluster - 0x80;
    if(writeSD(0, fatbase + sectorsPerFat * fatNum + (prevCluster >> 7))) return RC_DISK_ERR;

    fatRec = prevCluster & 0xFFFFFF80;
    for(unsigned char fatRemain = 0; fatRemain < 128; fatRemain++)
    {
      fatRec++;
      if(writeSD((unsigned char*)&fatRec, sizeof(fatRec))) return RC_DISK_ERR;
    }

    // finalize sector writing
    if (writeSD(0, 0)) return RC_DISK_ERR;
  }
  // prepare sector for writing
  if(writeSD(0, fatbase + sectorsPerFat * fatNum + (cluster >> 7))) return RC_DISK_ERR;

  fatRec = cluster & 0xFFFFFF80;
  for(unsigned char fatRemain = 0; fatRemain < (cluster & 0x7F); fatRemain++)
  {
    fatRec++;
    if(writeSD((unsigned char*)&fatRec, sizeof(fatRec))) return RC_DISK_ERR;
  }

  fatRec = 0x0FFFFFFF; // end of cluster chain
  if(writeSD((unsigned char*)&fatRec, sizeof(fatRec))) return RC_DISK_ERR;

  // finalize sector writing
  if (writeSD(0, 0)) return RC_DISK_ERR;

  return RC_OK;
}

TinySDLog::ResultCode TinySDLog::updateFatSector()
{
  for(unsigned char i = 0; i < numOfFATs; i++)
  {
      ResultCode res = updateSingleFatSector(i);
      if(res) return res;
  }
  return RC_OK;
}

TinySDLog::ResultCode TinySDLog::initLogFile()
{
  unsigned char fileInfo[32];
  if(readSD(fileInfo, clust2sect(dirbase) + logFileInfoSector, 0, sizeof(fileInfo))) return RC_DISK_ERR;
  bool fileFound = true;
  for(unsigned char i = 0; i < 11; i++)
  {
    if(fileInfo[i] != logFileInfo[i])
    {
      fileFound = false;
      break;
    }
  }
  if(fileFound)
  {
    logFileSize = LD_DWORD(fileInfo + DIR_FileSize);
  }
  else
  {
    logFileSize = 0;
    ResultCode res = updateLogFileInfo();
    if(res) return res;
    res = updateFatSector();
    if(res) return res;
  } 
  return RC_OK;
}

TinySDLog::ResultCode TinySDLog::close()
{
  char buf = ' ';
  if(logFileSize & 0x1FF)
  {
    for(unsigned short i = 0; i < 511 - (logFileSize & 0x1FF); i++)
    {
      if (writeSD(&buf, 1)) return RC_DISK_ERR;
    }
    buf = '\n';
    if (writeSD(&buf, 1)) return RC_DISK_ERR;
    // finalize sector writing
    if (writeSD(0, 0)) return RC_DISK_ERR;
    logFileSize = (logFileSize & 0xFFFFFE00) + 0x200;
    return updateLogFileInfo();
  }
  return RC_OK;
}

TinySDLog::ResultCode TinySDLog::writeLogFile(const void* bufPtr, unsigned int bufSize)
{
  ResultCode res;
  
  if (!database) return RC_NOT_ENABLED;   /* Check file system */

  while(bufSize)
  {
    if((logFileSize & 0x1FF) == 0)
    {
      if((logFileSize % (csize * 512)) == 0)
      {
        res = updateFatSector();
        if(res) return res;
      }
      // prepare sector for writing
      if(writeSD(0, database + (logFileFirstCluster - 2) * csize + (logFileSize >> 9))) return RC_DISK_ERR;
    }
    unsigned short blockSize = 512 - (logFileSize & 0x1FF);
    if(blockSize > bufSize) blockSize = bufSize;
    if(writeSD(bufPtr, blockSize)) return RC_DISK_ERR;
    bufSize -= blockSize;
    bufPtr += blockSize;
    logFileSize += blockSize;
    if((logFileSize & 0x1FF) == 0)
    {
      // finalize sector writing
      if (writeSD(0, 0)) return RC_DISK_ERR;
      res = updateLogFileInfo();
      if(res) return res;
    }
  }
  
  return RC_OK;
}

size_t TinySDLog::write(uint8_t b)
{
  return writeLogFile(&b, 1) ? 0 : 1;
}

#ifdef TINY_SD_LOGGER_RTC
void TinySDLog::print2digits(int number)
{
  if (number >= 0 && number < 10) write('0');
  print(number);
}
#endif

// writes timestamp to log file with format: DD-MM-YYYY HH:MM:SS
bool TinySDLog::writeTimestamp(void)
{
#ifdef TINY_SD_LOGGER_RTC
  tmElements_t tm;
  if(!RTC.read(tm)) return false; // TODO: check RTC.chipPresent()
  print2digits(tm.Day);
  write('-');
  print2digits(tm.Month); 
  write('-');
  print(tmYearToCalendar(tm.Year));
  write(' ');
  print2digits(tm.Hour);
  write(':');
  print2digits(tm.Minute);
  write(':');
  print2digits(tm.Second);
  write(' ');
#endif
  return true;
}

TinySDLog::ResultCode TinySDLog::init()
{
  ResultCode res;
  for(unsigned char attempt = 0; attempt < 3; attempt++)
  {
    res = mount();
    if(res) continue;
    res = initLogFile();
    if(res) continue;
    break;
  }
  return res;
}
