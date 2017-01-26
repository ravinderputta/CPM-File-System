#include <stdint.h> 
#include <stdio.h> 
#include "diskSimulator.h"
// first index is sector number, second index is byte in the block of bytes
static uint8_t disk[256][1024]; 


int blockRead(uint8_t *buffer,uint8_t blockNum) { 
  int i = 0; 
  for (i=0; i<1024;i++) { 
    *(buffer+i) = disk[(int) blockNum][(int) i];
  } 
  return 0; 
} 

int  blockWrite(uint8_t *buffer,uint8_t blockNum) { 
  int  i = 0; 
  for (i=0; i<1024;i++) { 
    disk[(int) blockNum][i] = *(buffer+i);
  } 
  return 0; 
}

// for debugging purposes 
void printBlock(uint8_t blockNum) { 
  int i;
  fprintf(stdout,"\nDISK BLOCK %x:\n",blockNum); 
  for (i = 0; i < BLOCK_SIZE; i++) { 
    if (i % 16 == 0) { 
      fprintf(stdout,"%4x: ",i); 
    }
    fprintf(stdout, "%2x ",disk[(int) blockNum][i]);
    if (i % 16 == 15) { 
      fprintf(stdout,"\n"); 
    }
  }
  fprintf(stdout,"\n"); 
} 

// read and write whole disk image, byte for byte from a Unix file
size_t writeImage(char *fileName) { 
  FILE *fp;
  size_t bytesWritten = -1; 
  fp = fopen(fileName,"w");
  bytesWritten = fwrite(disk,BLOCK_SIZE,NUM_BLOCKS,fp); 
  fclose(fp); 
  return bytesWritten; 
} 

size_t readImage(char *fileName) { 
  FILE *fp; 
  size_t bytesRead = -1; 
  fp = fopen(fileName,"r"); 
  // C uses row major order for multi dim arrays
  bytesRead = fread(disk,BLOCK_SIZE,NUM_BLOCKS,fp); 
  fclose(fp); 
  return bytesRead; 
  
} 
