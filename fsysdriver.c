#include <stdio.h> 
#include <stdint.h> 
#include "cpmfsys.h" 
#include "diskSimulator.h"

// for debugging, prints a region of memory starting at buffer with 
void printBuffer(uint8_t buffer[],int size) { 
  int i;
  fprintf(stdout,"\nBUFFER PRINT:\n"); 
  for (i = 0; i < size; i++) { 
    if (i % 16 == 0) { 
      fprintf(stdout,"%4x: ",i); 
    }
    fprintf(stdout, "%2x ",buffer[i]);
    if (i % 16 == 15) { 
      fprintf(stdout,"\n"); 
    }
  }
  fprintf(stdout,"\n"); 
} 


int main(int argc, char * argv[]) { 
  uint8_t buffer1[BLOCK_SIZE],buffer2[BLOCK_SIZE]; 
  int i; 
  readImage("image1.img"); 
  makeFreeList(); 
  cpmDir(); 
  printFreeList(); 
  cpmDelete("shortf.ps");
  cpmDir();
  cpmRename("mytestf1.txt","mytest2.tx");
  fprintf(stdout,"cpmRename return code = %d,\n",cpmRename("mytestf","mytestv2.x")); 
  cpmDir(); 
  printFreeList(); 
}

