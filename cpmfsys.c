#include "cpmfsys.h"
#include <ctype.h>
#include "diskSimulator.h"

bool freeList[NUM_BLOCKS];
uint8_t *globalBlockReadBuffer;

int findNumberOfFreeBlocks()
{
	int numberOfFreeBlocks=0;
	for(int i=0;i<sizeof(freeList);i++)
	{
		if(freeList[i])
		{
			numberOfFreeBlocks++;
		}
	}
	return numberOfFreeBlocks;
}

int numberOfBlocksInUse(int extentIndex)
{
	uint8_t *blockReadBuffer=malloc(1024);
	int numberOfBlocksCounter=0;
	blockRead(blockReadBuffer,0);
	for(int i=16;i<32;i++)
	{
		if(blockReadBuffer[(32*extentIndex)+i]>0 && blockReadBuffer[(32*extentIndex)+i]<256)
		{
			numberOfBlocksCounter++;
		}
	}
	return numberOfBlocksCounter;
}

int firstFreeExtentAvailable()
{
	uint8_t *blockReadBuffer=malloc(1024);
	blockRead(blockReadBuffer,0);
	for(int extentsIndex=0;extentsIndex<32;extentsIndex++)
	{
		if(blockReadBuffer[extentsIndex*32]==0xe5)
		{
			return extentsIndex;
		}
	}
	return -1;
}

int *firstNFreeBlocks(int n)
{
	int j=0;
	int* arr = malloc(sizeof(arr)*n);
	for(int i=1;i<256;i++)
	{
		if(freeList[i]==true)
		{
			arr[j++]=i;
			n--;
		}
		if(n<1)
		{
			break;
		}
	}
	return arr;
}

int firstNonZeroBlock(int extent)
{
	uint8_t *blockReadBuffer=malloc(1024);
	blockRead(blockReadBuffer,0);
	for(int i=extent*32+16;i<extent*32+32;i++)
	{
		if(blockReadBuffer[i]>0 && blockReadBuffer[i]<0)
		{
			return blockReadBuffer[i];
		}
	}
	return -1;
}

DirStructType  *mkDirStruct(int index,uint8_t *e) {
  char c;
  DirStructType *d;
  int i;
  d = malloc(sizeof(DirStructType));
  d->status = (e+index*EXTENT_SIZE)[0];
  int offset = 1;
  if (debugging) fprintf(stderr,"EXTENT %x, status=%x\n",index,d->status);
  do  {
    // name must have one non-blank character
    // characters after name padded with blanks
    c = (e+index*EXTENT_SIZE)[offset];
    (d->name)[offset-1] = c;
    offset++;
  } while (offset < 9 && c != ' ');
  if ((offset == 9) && (c != ' ')){ // 8-character name
    d->name[offset-1] = '\0';
  } else {  // shorter than 8-char name
    d->name[offset-2] = '\0'; // string terminator
  }
  offset = 9;
  do  {
    // extension need not have any non-blank chars
    c = (e+index*EXTENT_SIZE)[offset];
       d->extension[offset-9] = c;
       offset++;
  } while (offset < 12 && c != ' ');
  if (c  == ' ') {
    d->extension[offset-10] = '\0';
  } else {
    d->extension[offset-9] = '\0'; // string terminator
  }
  if (debugging) fprintf(stdout,"name: %s.%s\n",d->name,d->extension);
  d->XL = (e+index*EXTENT_SIZE)[12];
  d->BC = (e+index*EXTENT_SIZE)[13];
  d->XH = (e+index*EXTENT_SIZE)[14];
  d->RC = (e+index*EXTENT_SIZE)[15];
  memcpy(d->blocks,e+index*EXTENT_SIZE+16,16);
  // for now we assume one extent per file
  // ? does numbering start at 1 or zero
  // most commonly zero, but it was not standard
  if (debugging) {
    fprintf(stdout,"XL=%x ,RC=%x ,XH=%x ,BC=%x\n",d->XL,d->RC,d->XH,d->BC);
    fprintf(stdout,"BLOCKS: ");
    for (i=0;i<BLOCKS_PER_EXTENT;i++) {
      fprintf(stdout,"%x ",d->blocks[i]);
    }
    fprintf(stdout,"\n");
  }
  return d;
}


void writeDirStruct(DirStructType *d,uint8_t index, uint8_t *e) {
  int i;
  int extchar;
  // status byte
  (e+index*EXTENT_SIZE)[0] = d->status;
  // file name
  i = 1;
  while(d->name[i-1] != '\0' && d->name[i-1] != '.') {
    (e+index*EXTENT_SIZE)[i] = d->name[i-1];
    i++;
  }
  // pad with blanks
  for(;i<9;i++) {  // name is bytes 1-9
    (e+index*EXTENT_SIZE)[i] = ' ';
  }
  // file extension
  extchar = 0;
  while(d->extension[extchar] != '\0') {
    (e+index*EXTENT_SIZE)[i] = d->extension[extchar];
    i++;  extchar++;
  }
  // pad with blanks
  for(;i<12;i++) { // extension is bytes 10-12
    (e+index*EXTENT_SIZE)[i] = ' ';
  }
  // last block size and extent numbers
  (e+index*EXTENT_SIZE)[12] = d->XL;
  (e+index*EXTENT_SIZE)[13] = d->BC;
  (e+index*EXTENT_SIZE)[14] = d->XH;
  (e+index*EXTENT_SIZE)[15] = d->RC;
  //file block list
  memcpy(e+index*EXTENT_SIZE+16,d->blocks,16);

}


void makeFreeList() {
  uint8_t extentBuffer[BLOCK_SIZE];
  DirStructType *d;
  int i,j; // index vars
  // initially set all blocks as unused
  for (i=0;i<NUM_BLOCKS;i++) {
    freeList[i] = true;
  }
  // read the directory block from the disk
  blockRead(extentBuffer,(uint8_t) 0);
  for (i=0;i<BLOCK_SIZE/EXTENT_SIZE;i++) {
    // grab a directory entry/extent
    d = mkDirStruct(i,extentBuffer);
    // if it is used, mark all its blocks
    if (d->status != 0xe5) {
      for (j=0;j<16;j++) {
	if (d->blocks[j] != 0) {
          //fprintf(stderr,"marking block %x false\n",d->blocks[j]);
	  freeList[(int) d->blocks[j]] = false;
	}
      }
    }
  }
  freeList[0] = false; // directory block is never free
  if (debugging) printFreeList();
}


void printFreeList() {
  fprintf(stdout,"FREE BLOCK LIST: (* means in-use)\n");
  for (int i = 0; i < 16; i++) {
    fprintf(stdout,"%3x: ",i*16);
    for (int j = 0; j< 16; j++) {
      if (freeList[i*16+j]) {
	fprintf(stdout,". ");
      } else {
	fprintf(stdout,"* ");
      }
    }
    fprintf(stdout,"\n");
  }
}

int findExtentWithName(char *name, uint8_t *block0) {
  // split up the name into 8-char name and extension
  char namebuf[9];
  char extbuf[4];
  int i = 0;
  if(checkLegalName(name)==false)
  {
	  return -2;
  }
  // no blanks, no punctuation, no control chars
  while ( name[i] != '\0' && name[i] != '.'&& i < 8) {
//    if (name[i] < 48 || (name[i] > 57 && name[i] < 65) ||
//        (name[i] > 90 && name[i] < 97) || (name[i] > 122)) {
//  	return -2; // illegal character in name
//    }
    namebuf[i] = name[i];
    i++;
  }
  namebuf[i] = '\0'; // terminate the namebuf string
    // you have either read too many characters or hit the . or hit the end
  if (i == 8 && name[i] != '.') {  // name too long
    // name too long
	  //printf("If i==8 executed\n");
	  if(name[i]!='\0')
	  {
		  return -2;
	  }
  } else if (name[i] == '.') { // need to process the extension
    int extchar = 0; i++ ;  // start at 0 in extbuf, get past the . in name
    while ( name[i] != '\0' && extchar  < 3) {
      if (name[i] < 48 || (name[i] > 57 && name[i] < 65) ||
        (name[i] > 90 && name[i] < 97) || (name[i] > 122)) {
	return -2; // illegal character in ext
      }
      extbuf[extchar++] = name[i++];
    }


    extbuf[extchar] = '\0'; // terminate extbuf string

    if (extchar == 3 && name[i] != '\0') {
    	//printf("In find extent ext too long executed\n");// ext too long
      return -1;
    }
  }  else { // legal name, no extension
      extbuf[0] = '\0';
  }
  // search through the directory sector for a file with right name, ext
  for (i=0; i<BLOCK_SIZE/EXTENT_SIZE;i++) {
    DirStructType *d;
    d = mkDirStruct(i,block0);
    if (!strcmp(d->name,namebuf) && !strcmp(d->extension,extbuf)) {
      // found it
      if (d->status == 0xe5) {
    	  //printf("In find extent delete executed\n");
	return -1; // check if file deleted
      }
      return i;
    }
  }
  //printf("In find extent file not found executed\n");
  return -1; // file just not found in directory block
}

bool checkLegalName(char *name) {
  int i = 0; // index through the name
  int extchar = 0; // count length of the extension
  // no blanks, no punctuation, no control chars
  while ( name[i] != '\0' && name[i] != '.'&& i < 8) {
    if (name[i] < 48 || (name[i] > 57 && name[i] < 65) ||
        (name[i] > 90 && name[i] < 97) || (name[i] > 122)) {
  	return false; // illegal character in name
    }
    i++;
  }
  // you have either read too many characters or hit the . or hit the end
  if (i == 8 && name[i] != '.' && name[i] != '\0') {  // name too long
    // name too long
    return false;
  } else if (name[i] == '.') { // need to process the extension
    i++ ;  //  get past the . in name
    extchar = 0;
    // no blanks, control chars, or punctuation in the extension
    while ( name[i] != '\0' && extchar  < 3) {
      if (name[i] < 48 || (name[i] > 57 && name[i] < 65) ||
        (name[i] > 90 && name[i] < 97) || (name[i] > 122)) {
	return false; // illegal character in ext
      }
      i++;
      extchar++;
    }
    if (extchar == 3 && name[i] != '\0') {  // ext too long
      return false;
    }
  }
  return true;
}



void cpmDir() {
  int currFileSize = 0;
  DirStructType *d;
  uint8_t extentBuffer[BLOCK_SIZE];
  // grab the directory block from the disk
  blockRead(extentBuffer,(uint8_t) 0);
  // iterate through the directory
  fprintf(stdout,"DIRECTORY LISTING\n");
  for (int i=0;i<BLOCK_SIZE/EXTENT_SIZE;i++) {
    // grab a directory entry/extent
    d = mkDirStruct(i,extentBuffer);
    currFileSize= 0;
    // figure out how many full blocks it uses
    if (d->status != 0xe5) {
      for (int j=0;j<16;j++) {
	if (d->blocks[j] != 0) {
          currFileSize+=BLOCK_SIZE;
	}
      }
      currFileSize=currFileSize-BLOCK_SIZE+((int) d->RC)*128+(int)d->BC;
      // last block is partial, size given by RC, BC
      fprintf(stdout,"%s.%s %d\n",d->name,d->extension,currFileSize);
    }

  }
}


int cpmDelete(char *name) {
  uint8_t block0[1024];
  int index;
  DirStructType *d;
  // read the directory from the disk
  blockRead(block0,(uint8_t) 0);
  index = findExtentWithName(name,block0);
  if (index < 0 ) {
    return index; // file not found
  } else { // file found
    // mark the used  blocks free in the freelist
    d = mkDirStruct(index,block0);
    for (int i=0;i<16;i++) {
      if (d->blocks[i] != 0) {
	freeList[d->blocks[i]] = true;
        // no need to overwrite them, but we do, for security
        d->blocks[i] = 0;
      }
    }
    // overwrite the  with a blank extent
    block0[index*EXTENT_SIZE] = 0xe5; // unused
    // no need to overwrite the data; findExtentWithName checks status byte,
    // but we do it anyway
    // write the modified directory back to the disk
    blockWrite(block0,(uint8_t) 0);
  return 0;
  }
}

int cpmRename(char *oldName, char * newName)
{
	uint8_t *blockReadBuffer=malloc(1024);
	blockRead(blockReadBuffer,0);
	int extentNumber = findExtentWithName(oldName,blockReadBuffer);
	if(!checkLegalName(oldName) || !checkLegalName(newName))
	{
		return -2;
	}
	else if(extentNumber==-1)
	{
		return -1;
	}

	else if(findExtentWithName(newName,blockReadBuffer)!=-1)
	{
		return -3;
	}
	else
	{
		//Check whether Dot is present or not in the new name
		char *dotPointer=strchr(newName,'.');
		int indexOfDot=(int)(dotPointer-newName);
		if(dotPointer)
		{
			//printf("Dot Present Rename %d\n",indexOfDot);
			int fileStartIndex=extentNumber*32+1;
			int extStartIndex=extentNumber*32+9;
			for(int i=0;i<indexOfDot;i++)
			{
				//printf("first for loop\n");
				blockReadBuffer[fileStartIndex++]=newName[i];
			}
			for(int j=indexOfDot+1;j<strlen(newName);j++)
			{
				blockReadBuffer[extStartIndex++]=newName[j];
			}
			for(int k=fileStartIndex;k<extentNumber*32+9;k++)
			{
				blockReadBuffer[k]=0;
			}
			for(int l=extStartIndex;l<extentNumber*32+12;l++)
			{
				blockReadBuffer[l]=0;
			}
			blockWrite(blockReadBuffer,0);
			//writeImage("image1.img");
			return 0;
		}
		else
		{
			int fileStartIndex=extentNumber*32+1;
			for(int i=0;i<strlen(newName);i++)
			{
				blockReadBuffer[fileStartIndex++]=newName[i];
			}
			blockWrite(blockReadBuffer,0);
			//writeImage("image1.img");
			return 0;
		}
	}
}

int  cpmCopy(char *oldName, char *newName)
{
	uint8_t *blockReadBuffer=malloc(1024);
	blockRead(blockReadBuffer,0);
	int extentNumber;
	if(checkLegalName(oldName)==false || checkLegalName(newName)==false)
	{
		return -2;
	}
	extentNumber=findExtentWithName(oldName,blockReadBuffer);
	if(findExtentWithName(oldName,blockReadBuffer)==-1)
	{
		return -1;
	}
	if(findExtentWithName(newName,blockReadBuffer)!=-1)
	{
		//printf(" -3 returned is %s %d",newName,findExtentWithName(newName,blockReadBuffer));
		return -3;
	}
	int numberOfBlocksRequired=numberOfBlocksInUse(findExtentWithName(oldName,blockReadBuffer));
	if(findNumberOfFreeBlocks()<numberOfBlocksRequired)
	{
		return -4;
	}
	int firstFreeExtent=firstFreeExtentAvailable();
	if(firstFreeExtent==-1)
	{
		return -5;
	}
	blockReadBuffer[firstFreeExtent*32]=1;
	char *dotPointer=strchr(newName,'.');
	int indexOfDot=(int)(dotPointer-newName);
	if(dotPointer)
	{
		int fileCopy=1;
		//Copying File Name
		for(int i=1;i<=indexOfDot;i++)
		{
			blockReadBuffer[firstFreeExtent*32+(fileCopy++)]=newName[i-1];
		}
		if(indexOfDot<8)
				{
					int initialValue=(firstFreeExtent*32)+8;
					for(int i=0;i<(8-indexOfDot);i++)
					{
						blockReadBuffer[initialValue--]=0;
						fileCopy++;
					}
				}
		//Copying Extension
		for(int i=indexOfDot+1;i<strlen(newName);i++)
		{
			blockReadBuffer[firstFreeExtent*32+(fileCopy++)]=newName[i];
		}

		if(strlen(newName)-indexOfDot<3)
		{
			int initialValue=(firstFreeExtent*32)+11;
			for(int i=0;i<(strlen(newName)-indexOfDot);i++)
			{
				blockReadBuffer[initialValue--]=0;
			}
		}
	}
	else
	{
		for(int i=0;i<strlen(newName);i++)
		{
			blockReadBuffer[(firstFreeExtent*32)+(i+1)]=newName[i];
		}
		int initialValue=(firstFreeExtent*32)+11;
		for(int i=0;i<(11-strlen(newName));i++)
		{
			blockReadBuffer[initialValue--]=0;
		}
	}

	for(int i=12;i<16;i++)
	{
		blockReadBuffer[(firstFreeExtent*32)+i]=blockReadBuffer[(extentNumber*32)+i];
	}
	int *freeBlocks=firstNFreeBlocks(numberOfBlocksRequired);
	for(int i=0;i<numberOfBlocksRequired;i++)
	{
		blockReadBuffer[(firstFreeExtent*32)+(i+16)]=freeBlocks[i];
		freeList[freeBlocks[i]]=false;
	}
	//if(numberOfBlocksRequired<16)
	//{
		for(int i=16+(numberOfBlocksRequired);i<=31;i++)
		{
			blockReadBuffer[(firstFreeExtent*32)+(i)]=0;
		}
	//}
	for(int i=0;i<16;i++)
	{
		uint8_t *blockReadBuffer1=malloc(1024);
		if(blockReadBuffer[(extentNumber*32)+(i+16)]!=0)
		{
			blockRead(blockReadBuffer1,blockReadBuffer[(extentNumber*32)+(i+16)]);
			blockWrite(blockReadBuffer1,blockReadBuffer[(firstFreeExtent*32)+(i+16)]);
		}
	}
	blockWrite(blockReadBuffer,0);
	return 0;
}

int  cpmOpen( char *fileName, char mode)
{
	if(!checkLegalName(fileName))
	{
		return -2;
	}
	if(mode!='r' && mode!='w')
	{
		return -6;
	}
	uint8_t *blockReadBuffer=malloc(1024);
	blockRead(blockReadBuffer,0);
	//int extentIndex=findExtentWithName(fileName,blockReadBuffer);
	//printf("findExtent returned in cpmOpen is: %s, %d\n",fileName,findExtentWithName(fileName,blockReadBuffer));
	if((mode=='r') && ((findExtentWithName(fileName,blockReadBuffer))==-1))
	{
		return -1;
	}
	for(int i=0;i<32;i++)
	{
		if(openFileTable[i]!=NULL && openFileTable[i]->directoryEntryIndex==findExtentWithName(fileName,blockReadBuffer))
		{
			return -7;
		}
	}
	if(mode=='w' && findNumberOfFreeBlocks()==0)
	{
		return -5;
	}
	for(int i=0;i<32;i++)
	{
		if(openFileTable[i]==NULL)
		{
			openFileTable[i]=malloc(sizeof(openFileTable[i]));
			int nonZeroBlock=firstNonZeroBlock(findExtentWithName(fileName,blockReadBuffer));
			openFileTable[i]->buffer=malloc(1024);
			if(mode=='r')
			{
				blockRead(openFileTable[i]->buffer,nonZeroBlock);
				//printArray();
			}
			else
			{
				blockWrite(openFileTable[i]->buffer,nonZeroBlock);
			}
			if(findExtentWithName(fileName,blockReadBuffer)!=-1)
			{
				openFileTable[i]->directoryEntryIndex=findExtentWithName(fileName,blockReadBuffer);
				openFileTable[i]->readWriteIndex=0;
				openFileTable[i]->mode=mode;
				openFileTable[i]->dirStruct=mkDirStruct(findExtentWithName(fileName,blockReadBuffer),blockReadBuffer);
				openFileTable[i]->currBlockIndex=nonZeroBlock;
				return i;
			}
			else if(findExtentWithName(fileName,blockReadBuffer)==-1)
			{
				char fileString[8];
				char extString[3];
				//Assign an extent
				int assign_extent=firstFreeExtentAvailable();
				globalBlockReadBuffer=malloc(1024);
				blockRead(globalBlockReadBuffer,0);
				globalBlockReadBuffer[assign_extent*32]=1;
				char *dotPointer=strchr(fileName,'.');
				int indexOfDot = (int)(dotPointer-fileName);
				if(dotPointer)
				{
					for(int i=0;i<indexOfDot;i++)
					{
						fileString[i]=fileName[i];
					}
					int internalK=0;
					for(int extIndex=indexOfDot+1;extIndex<strlen(fileName);extIndex++)
					{
						extString[internalK++]=fileName[extIndex];
					}
					for(int i=0;i<strlen(fileString);i++)
					{
						globalBlockReadBuffer[assign_extent*32+(i+1)]=fileString[i];
					}
					for(int i=0;i<strlen(extString);i++)
					{
						globalBlockReadBuffer[assign_extent*32+(i+9)]=extString[i];
					}
					//blockWrite(blockReadBuffer,0);
				}
				else
				{
					for(int i=0;i<strlen(fileName);i++)
					{
						globalBlockReadBuffer[assign_extent*32+(i+1)]=fileName[i];
					}
					//blockWrite(blockReadBuffer,0);
				}
				for(int i=0;i<20;i++)
				{
					globalBlockReadBuffer[assign_extent*32+(i+12)]=0;
				}
				blockWrite(globalBlockReadBuffer,0);
				openFileTable[i]->directoryEntryIndex=assign_extent;
				openFileTable[i]->readWriteIndex=0;
				openFileTable[i]->mode=mode;
				openFileTable[i]->dirStruct=mkDirStruct(assign_extent,globalBlockReadBuffer);
				openFileTable[i]->currBlockIndex=firstNonZeroBlock(assign_extent);
				return i;
			}
		}
	}
	return -13;
}

void printOpenFileTable()
{
	for(int i=0;i<32;i++)
	{
		if(openFileTable[i]!=NULL)
		{
			printf("Mode is: %c\n",openFileTable[i]->mode);
			printf("File Name is: %s\n",openFileTable[i]->dirStruct->name);
		}
	}
}

int cpmClose(int filePointer)
{
	if(filePointer<0 || filePointer>31)
	{
		return -8;
	}
	if(openFileTable[filePointer]!=NULL)
	{
		openFileTable[filePointer]=NULL;
	}
	return 0;
}

int cpmRead(int pointer, uint8_t *buffer, int size)
{
	if(openFileTable[pointer]==NULL)
	{
		return -8;
	}
	if(openFileTable[pointer]!=NULL && openFileTable[pointer]->mode!='r')
	{
		return -6;
	}
	if(size>1024)
	{
		return -11;
	}
	int nonZeroBlocks=numberOfBlocksInUse(openFileTable[pointer]->directoryEntryIndex);
	int sizeOfFile=(nonZeroBlocks-1)*1024;
	sizeOfFile+=(openFileTable[pointer]->dirStruct->BC)*128+openFileTable[pointer]->dirStruct->RC;
	if(size>sizeOfFile-(openFileTable[pointer]->readWriteIndex))
	{
		return -9;
	}
	int nonZeroBlocksArray[nonZeroBlocks];
	uint8_t *blockReadBuffer=malloc(1024);
	int nonZeroBlocksArrayIndex=0;
	for(int i=0;i<16;i++)
	{
		if(openFileTable[pointer]->dirStruct->blocks[i]>0 && openFileTable[pointer]->dirStruct->blocks[i]<256)
		{
			nonZeroBlocksArray[nonZeroBlocksArrayIndex++]=openFileTable[pointer]->dirStruct->blocks[i];
		}
	}
	blockRead(blockReadBuffer,nonZeroBlocksArray[(int)((openFileTable[pointer]->readWriteIndex)/1024)]);
	//printBlock(nonZeroBlocksArray[(int)((openFileTable[pointer]->readWriteIndex)/1024)]);
	int readIndex=0;
	int blockReaderBufferIndex=(openFileTable[pointer]->readWriteIndex)%1024;
	while(blockReaderBufferIndex<1024 && readIndex<size)
	{
		//printf("In read %x \n",blockReadBuffer[blockReaderBufferIndex]);
		buffer[readIndex++]=blockReadBuffer[blockReaderBufferIndex++];
		openFileTable[pointer]->readWriteIndex++;
	}
	if(blockReaderBufferIndex==1024 && readIndex<size)//Read operation crossed block boundaries
	{
		blockReaderBufferIndex=0;
		uint8_t *blockReadBuffer1=malloc(1024);
		blockRead(blockReadBuffer1,nonZeroBlocksArray[(int)((openFileTable[pointer]->readWriteIndex)/1024)]);
		while(readIndex<size)
		{
			buffer[readIndex++]=blockReadBuffer1[blockReaderBufferIndex++];
			openFileTable[pointer]->readWriteIndex++;
		}
	}
	//printf("Read Write Index is %d\n",openFileTable[pointer]->readWriteIndex);
	return 0;
}

int cpmWrite(int pointer, uint8_t *buffer, int size)
{
	if(openFileTable[pointer]==NULL)
	{
		return -8;
	}
	if(openFileTable[pointer]!=NULL && openFileTable[pointer]->mode!='w')
	{
		return -6;
	}
	//calculate file size associated with the file pointer
	int fileSize=0;
	for(int i=0;i<16;i++)
	{
		if(openFileTable[pointer]->dirStruct->blocks[i]!=0)
		{
			fileSize+=1024;
		}
	}
	fileSize-=1024;
	//printf("File size is %d\n",fileSize);
	int lastBlockSize=128*(openFileTable[pointer]->dirStruct->RC)+openFileTable[pointer]->dirStruct->BC;
	fileSize+=lastBlockSize;
	if(size>((16*1024)-fileSize))
	{
		return -10;
	}
	//calculate available disk space associated with the file pointer
	long availableDiskSpace=0;
	for(int i=0;i<256;i++)
	{
		if(freeList[i]==true)
		{
			availableDiskSpace+=1024;
		}
	}
	if(size>availableDiskSpace)
	{
		return -4;
	}
	if(size>1024)
	{
		return -11;
	}

	int firstEmptyBlockIndex;
	for(firstEmptyBlockIndex=0;firstEmptyBlockIndex<16;firstEmptyBlockIndex++)
	{
		//printf("open FIle table ka blocks %d\n",openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex]);
		if(openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex]==0)
		{
			break;
		}
	}
	if(firstEmptyBlockIndex>0)
	{
		firstEmptyBlockIndex--;
	}
	//printf("first Empty Block Index %d\n", firstEmptyBlockIndex);
	uint8_t *blockWriteBuffer=malloc(1024);
	int currentBlock;
	if(firstEmptyBlockIndex==0 && openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex]==0)//indicates new file
	{
		int *a;
		a=firstNFreeBlocks(1);
		currentBlock=a[0];
		//printf("First free list %d\n",currentBlock);
		freeList[currentBlock]=false;
		openFileTable[pointer]->dirStruct->blocks[0]=currentBlock;
		blockRead(blockWriteBuffer,currentBlock);
	}
	else
	{
		currentBlock=openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex];
		blockRead(blockWriteBuffer,currentBlock);
	}
	int blockWriteBufferIndex=0;
	int writeIndex=0;
	int lastBlockIndex=lastBlockSize;
	openFileTable[pointer]->currBlockIndex=currentBlock;
	while(writeIndex<size && lastBlockIndex<1024)
	{
		blockWriteBuffer[lastBlockIndex++]=buffer[blockWriteBufferIndex++];
		writeIndex++;
		openFileTable[pointer]->readWriteIndex++;
	}
	if(writeIndex==size)
	{
		uint8_t *blockReadBuffer=malloc(1024);
		blockRead(blockReadBuffer,0);
		openFileTable[pointer]->dirStruct->RC=(int)writeIndex/128;
		openFileTable[pointer]->dirStruct->BC=(writeIndex-(128*(openFileTable[pointer]->dirStruct->RC)));
		blockReadBuffer[(openFileTable[pointer]->directoryEntryIndex)*32+0]=1;
		blockReadBuffer[(openFileTable[pointer]->directoryEntryIndex)*32+13]=openFileTable[pointer]->dirStruct->BC;
		blockReadBuffer[(openFileTable[pointer]->directoryEntryIndex)*32+15]=openFileTable[pointer]->dirStruct->RC;
		blockReadBuffer[(openFileTable[pointer]->directoryEntryIndex)*32+16]=currentBlock;
		blockWrite(blockReadBuffer,0);
		blockWrite(blockWriteBuffer,currentBlock);
		return 0;
		//printf("BC value is %d\n",openFileTable[pointer]->directoryEntryIndex);
		//writeImage("ec-final-v2.img");
		//mkDirStruct(openFileTable[pointer]->directoryEntryIndex,blockReadBuffer);
	}
	//printBlock(openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex]);
	else if(writeIndex<size && lastBlockIndex==1024)//Boundary condition reached, get another block to continue right
	{
		uint8_t *blockReadBufferbb=malloc(1024);
		blockRead(blockReadBufferbb,0);
		openFileTable[pointer]->dirStruct->RC=0;
		//printf("RC before boundary is %d\n",openFileTable[pointer]->dirStruct->RC);
		//printf("BC before boundary is %d\n",openFileTable[pointer]->dirStruct->BC);
		openFileTable[pointer]->dirStruct->BC=0;
		//printf("BC before boundary is %d\n",openFileTable[pointer]->dirStruct->BC);
		blockReadBufferbb[(openFileTable[pointer]->directoryEntryIndex)*32+0]=1;
		blockReadBufferbb[(openFileTable[pointer]->directoryEntryIndex)*32+13]=openFileTable[pointer]->dirStruct->BC;
		blockReadBufferbb[(openFileTable[pointer]->directoryEntryIndex)*32+15]=openFileTable[pointer]->dirStruct->RC;
		blockReadBufferbb[(openFileTable[pointer]->directoryEntryIndex)*32+16]=currentBlock;
		blockWrite(blockReadBufferbb,0);
		blockWrite(blockWriteBuffer,currentBlock);

		//Get another block from freeList
		int blockAfterBoundary;
		for(int i=0;i<256;i++)
		{
			if(freeList[i]==true)
			{
				blockAfterBoundary=i;
				freeList[i]=false;
				break;
			}
		}
		//put that block into the file Pointer extent
		openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex+1]=blockAfterBoundary;
		//printf("New block after boundary is %d\n",blockAfterBoundary);
		//read that block into a buffer and then start writing
		blockRead(blockWriteBuffer,openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex+1]);
		openFileTable[pointer]->currBlockIndex=openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex+1];
		lastBlockIndex=0;
		int diff=size-writeIndex;
		while(writeIndex<size && blockWriteBufferIndex<1024)
		{
			blockWriteBuffer[lastBlockIndex++]=buffer[blockWriteBufferIndex++];
			writeIndex++;
			openFileTable[pointer]->readWriteIndex++;
		}
		uint8_t *blockReadBufferab=malloc(1024);
		blockRead(blockReadBufferab,0);
		openFileTable[pointer]->dirStruct->RC+=(int)diff/128;
		openFileTable[pointer]->dirStruct->BC+=(diff-(128*((int)diff/128)));
		blockReadBufferab[(openFileTable[pointer]->directoryEntryIndex)*32+0]=1;
		blockReadBufferab[(openFileTable[pointer]->directoryEntryIndex)*32+13]=openFileTable[pointer]->dirStruct->BC;
		blockReadBufferab[(openFileTable[pointer]->directoryEntryIndex)*32+15]=openFileTable[pointer]->dirStruct->RC;
		blockReadBufferab[(openFileTable[pointer]->directoryEntryIndex)*32+17]=blockAfterBoundary;
		blockWrite(blockReadBufferab,0);
		blockWrite(blockWriteBuffer,openFileTable[pointer]->dirStruct->blocks[firstEmptyBlockIndex+1]);
		return 0;
	}
	return 0;
}
