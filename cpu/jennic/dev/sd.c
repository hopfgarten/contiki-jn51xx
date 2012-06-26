/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2009      */
/*-----------------------------------------------------------------------*/

#include "sd.h"
#include "contiki.h"
#include "AppHardwareApi.h"

#define LOWER_CS() do { vAHI_SpiWaitBusy(); vAHI_SpiSelect(1<<1); vAHI_SpiWaitBusy(); } while(0)
#define RAISE_CS() do { vAHI_SpiWaitBusy(); vAHI_SpiSelect(0); vAHI_SpiWaitBusy(); } while(0)

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

#define DEBUG 0

#include <stdio.h>
#if DEBUG != 0
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifndef MIN 
#define MIN(a, b)_((a) < (b) ? (a) : (b))
#endif /* MIN */

#define SDCARD_SLAVE E_AHI_SPIM_SLAVE_ENBLE_0

/* SD commands */
#define GO_IDLE_STATE     0
#define SEND_OP_COND      1
#define SWITCH_FUNC       6
#define SEND_IF_COND      8
#define SEND_CSD          9
#define SEND_CID          10
#define STOP_TRANSMISSION 12
#define SEND_STATUS       13
#define SET_BLOCKLEN      16
#define READ_SINGLE_BLOCK 17
#define WRITE_BLOCK       24
#define APP_CMD           55
#define READ_OCR          58
#define SPI_IDLE          0xff

#define APP_SEND_OP_COND  41

/* SD response lengths. */
#define R1 1
#define R2 2
#define R3 5
#define R7 5

#define START_BLOCK_TOKEN 0xfe

/* Status codes returned after writing a block. */
#define DATA_ACCEPTED 2
#define DATA_CRC_ERROR 5
#define DATA_WRITE_ERROR 6

#define SD_TRANSACTION_ATTEMPTS		1024
#define SD_READ_RESPONSE_ATTEMPTS	8
#define SD_READ_BLOCK_ATTEMPTS		2

#define SD_DEFAULT_BLOCK_SIZE		512

static uint16_t address_mult;
static uint8_t inited = 0;

static void command(uint8_t cmd, uint32_t argument)
{
  uint8_t req[6], i;

  req[0] = 0x40 | cmd;
  req[1] = argument >> 24;
  req[2] = argument >> 16;
  req[3] = argument >> 8;
  req[4] = argument;
  if (cmd == GO_IDLE_STATE)
    req[5] = 0x95;
  else if (cmd == SEND_IF_COND)
    req[5] = 0x87;
  else
    req[5] = 0xff;

  for (i=0; i<sizeof(req); i++) 
  {
    vAHI_SpiStartTransfer8(req[i]);
    vAHI_SpiWaitBusy(); 
  }
}

static void transaction(uint8_t cmd, uint32_t argument, uint8_t response_type, uint8_t * response)
{
  uint8_t i;

  LOWER_CS();
  
  /* send command */
  command(cmd, argument);

  /* wait for response */
  for (i = 0; i < SD_READ_RESPONSE_ATTEMPTS; i++)
  {
    vAHI_SpiStartTransfer8(SPI_IDLE);
    vAHI_SpiWaitBusy(); 
    response[0] = u8AHI_SpiReadTransfer8();
    if (!(response[0] & 0x80)) //check for response
      break;
  }
  for (i = 1; i < response_type; i++)
  {
    vAHI_SpiStartTransfer8(SPI_IDLE);
    vAHI_SpiWaitBusy(); 
    response[i] = u8AHI_SpiReadTransfer8();
  }
  RAISE_CS();

  /* one pause cycle */
  vAHI_SpiStartTransfer8(SPI_IDLE);
  vAHI_SpiWaitBusy();
}

/*init disk*/
DSTATUS disk_initialize (BYTE drive)
{
  uint8_t response[R7];
  uint16_t i;
  uint8_t sdhc = 0;
  uint8_t mmc = 0;

  /* only drive 0 allowed */
  if (drive)
    return STA_NODISK;

  /* init with slow SPI speed */
  vAHI_SpiConfigure(2,   // number of spi slave select line
                    E_AHI_SPIM_MSB_FIRST,
                    0,0, // polarity and phase
                    63,   // max clock divisor
                    E_AHI_SPIM_INT_DISABLE,
                    E_AHI_SPIM_AUTOSLAVE_DSABL);

  /* init cycles */
  for (i = 0; i < 100; i++)
  {
    vAHI_SpiStartTransfer8(0xff);
    vAHI_SpiWaitBusy();
  }

  /* software reset */
  transaction(GO_IDLE_STATE, 0, R1, response);
  if(!(response[0] & 0x01))
  {
    return STA_NOINIT;
    RAISE_CS();
  }

  /* SDHC test */
  transaction(SEND_IF_COND, 0x000001aa, R7, response);
  if (!(response[0] & 0x04) && response[3]  == 0x01 && response[4] == 0xaa)
    sdhc = 1;

  /* wake up */
  for (i = 0; i < SD_TRANSACTION_ATTEMPTS; i++)
  {
    if (mmc)
      transaction(SEND_OP_COND, 0, R1, response);
    else
    {
      transaction(APP_CMD, 0, R1, response);
      transaction(APP_SEND_OP_COND, 0x40000000 * sdhc, R1, response);
      if (response[0] & 0x04)
        mmc = 1;
    }
    if (!(response[0] & 0x01))
      break;
  }
  if (response[0] & 0x01)
  {
    return STA_NOINIT;
    RAISE_CS();
  }

  /* read OCR */
  transaction(READ_OCR, 0, R3, response);
  if (!(response[1] & 0x80))
  {
    return STA_NOINIT;
    RAISE_CS();
  }

  /* byte or block address */
  if (response[1] & 0x40)
    address_mult = 1;
  else
  {
    address_mult = SD_DEFAULT_BLOCK_SIZE;
    transaction(SET_BLOCKLEN, SD_DEFAULT_BLOCK_SIZE, R1, response);
  }    

  RAISE_CS();

  inited = 1;

  /* continue at high speed */
  vAHI_SpiConfigure(2,   // number of spi slave select line
                    E_AHI_SPIM_MSB_FIRST,
                    0,0, // polarity and phase
                    0,   // min clock divisor
                    E_AHI_SPIM_INT_DISABLE,
                    E_AHI_SPIM_AUTOSLAVE_DSABL);


  return 0;
}

DSTATUS disk_status (BYTE drive)
{
  if (drive)
    return STA_NODISK;
  else if (!inited)
    return STA_NOINIT;
  return 0;
}

/* read multiple sectors */
DRESULT disk_read (BYTE drive, BYTE* buffer, DWORD start_sector, BYTE sector_count)
{
  /* only drive 0 allowed */
  if (drive)
    return RES_PARERR;
  
  uint8_t temp;
  uint16_t i, n;
  for (n = 0; n < sector_count; n++)
  {
    uint32_t argument = (start_sector + n) * address_mult;

    LOWER_CS();

    command(READ_SINGLE_BLOCK, argument);
  
    /* wait for response */
    for (i = 0; i < SD_READ_RESPONSE_ATTEMPTS; i++) {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy(); 

      temp = u8AHI_SpiReadTransfer8();
      if (!temp)
        break;
    }
    if (temp) {
      RAISE_CS();
      return RES_NOTRDY;
    }

    /* wait for start token */
    for (i = 0; i < SD_TRANSACTION_ATTEMPTS; i++) {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy(); 
      temp = u8AHI_SpiReadTransfer8();
      if(temp == START_BLOCK_TOKEN || (temp > 0 && temp <= 8))
        break;
    }

    if (temp == START_BLOCK_TOKEN) {
      /* read requested bytes */
      for (i = 0; i < SD_DEFAULT_BLOCK_SIZE; i++) {
        vAHI_SpiStartTransfer8(SPI_IDLE);
        vAHI_SpiWaitBusy(); 
        buffer[i + (n * SD_DEFAULT_BLOCK_SIZE)] = u8AHI_SpiReadTransfer8();
      }

      /* crc bytes (todo: check crc) */
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy();
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy();

      RAISE_CS();

      /* one pause cycle */
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy(); 
      
    } 
    else 
    {
      RAISE_CS();
      return RES_ERROR;
    }
  }
  return RES_OK;
}


#if	_READONLY == 0
/* write multiple sectors */
DRESULT disk_write (BYTE drive, const BYTE* buffer, DWORD start_sector, BYTE sector_count)
{
  /* only drive 0 allowed */
  if (drive)
    return RES_PARERR;
  
  uint8_t temp = RES_OK;
  uint16_t i, n;
  for (n = 0; n < sector_count && temp == RES_OK; n++)
  {
    uint32_t argument = (start_sector + n) * address_mult;

    LOWER_CS();

    command(WRITE_BLOCK, argument);
  
    /* wait for response */
    for (i = 0; i < SD_READ_RESPONSE_ATTEMPTS; i++)
    {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy(); 
      temp = u8AHI_SpiReadTransfer8();
      if (!temp)
        break;
    }
    if (temp)
    {
      RAISE_CS();
      return RES_NOTRDY;
    }
      
    /* send some idles before start token (required?) */
    for (i = 0; i < 8; i++)
    {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy();
    }
      
    /* send start token */
    vAHI_SpiStartTransfer8(START_BLOCK_TOKEN);
    vAHI_SpiWaitBusy();
     
    /* send bytes */
    for (i = 0; i < SD_DEFAULT_BLOCK_SIZE; i++)
    {
      vAHI_SpiWaitBusy();
      vAHI_SpiStartTransfer8(buffer[i + (n * SD_DEFAULT_BLOCK_SIZE)]);
    }
    
    /* wait for data response token */
    for(i = 0; i < SD_TRANSACTION_ATTEMPTS; i++) 
    {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy();
      temp = u8AHI_SpiReadTransfer8();
      if ((temp & 0x11) == 1) 
      {
        /* Data response token received. */
        temp = (temp >> 1) & 0x7;
        if(temp == DATA_ACCEPTED) 
        {
          temp = RES_OK;
          break;
        } 
        else 
        {
          temp = RES_ERROR;
          break;
        }
      }
    }      

    /* wait until sd card is finished */
    while (u8AHI_SpiReadTransfer8() != SPI_IDLE) 
    {
      vAHI_SpiStartTransfer8(SPI_IDLE);
      vAHI_SpiWaitBusy();
    }

    RAISE_CS();
  }
  return temp;
}
#endif

/* not completed? */
DRESULT disk_ioctl (BYTE drive, BYTE command, void* buffer)
{
  if (command == CTRL_SYNC)
    return RES_OK;
  return RES_PARERR;
}




