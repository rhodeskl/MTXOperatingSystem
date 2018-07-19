/***** Lab 4 Prelab *****/


#include "type.h"

GD *gp;
SUPER *sp;
INODE *ip;
DIR *dp;

MINODE *minodes[NMINODE];
MINODE *root;
PROC   proc[NPROC], *running;
MTABLE *mtable[4]; 
MINODE*mip;

#define BLKSIZE 1024
#define EXT2 0xEF53
unsigned char buf[BLKSIZE];
int fd;
char * dev = "mydisk";
char *path[64];
char command[64];
char pathname[64];
char *cmd[] = { "menu", "ls", "cd", "pwd", "quit","mkdir","creat","rmdir", "unlink","link","symlink", "readlink", "touch", "chmod", "stat", "close", "lseek", "write","open", "printOpenFiles", "mv", "cp", "read", "cat", "mount", "umount", 0 };

int inode_count;
int inode_size;
int block_count;
int block_size;
int inode_bitmap;
int block_bitmap;
int iblock;

STAT *st;

int updateMipOfOpenFile(char *filename, MINODE *mip)
{
	int index = 0;
	OFT *oftp;
	while(index < NFD)
	{
		oftp = running -> fd[index];
		if((strcmp(oftp->filename, filename) == 0) && oftp -> mode == 2)
		{
			oftp -> mptr = mip;
			printf("values of mip in updateMipOfOpenFile"); //debugging
			printf("ino = %d\n", mip -> ino); //debugging
			printf("mounted = %d\n", mip -> mounted); //debugging
			printf("dev = %d\n", mip -> dev); //debugging
			printf("dirty = %d\n", mip -> dirty); //debugging
			running->fd[index] = oftp;
			return 1;
		}
		index++;
	}
	return 0;	
}

MINODE *getMipOfOpenFile(char *filename)
{
	int index = 0;
	OFT *oftp;
	while(index < NFD)
	{
		oftp = running -> fd[index];
		if((strcmp(oftp->filename, filename) == 0) && oftp -> mode == 2)
		{
			oftp -> mptr -> refCount++; //might be wrong, not sure
			return oftp -> mptr;
		}
		index++;
	}
	return 0;
}

int mount1(char *filesys, char *mount_point) //Usage: mount [filesys mount_point]
{
	/*
	1. If no parameter, display current mounted file systems
	2. Check whether filesys is already mounted:
		The MOUNT table entries contain mounted file system (device) names and their mounting points. Reject if the device is already mounted. If not, allocate a free MOUNT table entry
	3. filesys is a special file with a device number dev = (major, minor)
		Read filesys' superblock to verify it is an EXT2 FS
	4. find the ino, and then the minode of mount_point:
		call ino = get_ino(&dev, pathname); to get ino
		call mip = iget(dev, ino); to load its inode into memory
	5. check mount_point is a DIR and not busy, e.g. not someone's CWD
	6. record dev and filesys name in the MOUNT table entry; also store its ninodes, nblocks, etc. for quick reference
	7. Mark mount_point's minode as mounted on (mounted flag = 1) and let it point at the MOUNT table entry, which points back to the mount_point minode
	*/

	int index = 0, ino, dev, ninodes, nblock, bmap, imap, iblock;
	MINODE *mip;
	SUPER *sp;
	GD *gp;

	//step 1
	if(strcmp(filesys, "") == 0)
	{
		printf("the currently mounted file systems are:\n");
		printf("mountedDirName\tdeviceName\n");
		while(index < 4)
		{
			if(mtable[index] != NULL)
			{
				printf("%s\t%s\n", mtable[index]->mountedDirName, mtable[index]->deviceName);
			}
			index++;
		}
		return 0;
	}

	//step 2
	//printf("step 2, looking to see if %s already mounted\n", filesys); //debugging
	index = 0;
	while(index < 4)
	{
		if(mtable[index] != NULL)
		{
			if(strcmp(filesys, mtable[index]->deviceName) == 0)
			{
				printf("%s is already mounted\n", filesys);
				return 0;
			}
		}
		index++;
	}

	//step 3
	//printf("step 3\n"); //debugging
	//printf("opening %s for RW\n", filesys); //debugging
	dev = open1(filesys, "RW");
	//printf("%s open for RW, dev = %d\n", filesys, dev); //debugging
	if(dev < 0)
	{
		printf("%s does not exist\n", filesys);
		return 0;
	}

	//printf("getting superblock\n"); //debugging
	//get_block(dev,1,buf);
	lseek1(dev, BLKSIZE);
	read1(dev, buf, 1024);
	sp =(SUPER*)buf;
	if(sp->s_magic != 0xEF53)
	{
		printf("%s is not an EXT2 filesystem, cannot mount\n", filesys);
		return 0;
	}
	ninodes = sp->s_inodes_count;
	nblock = sp->s_blocks_count;
	//printf("from superblock: ninodes = %d, nblock = %d\n", ninodes, nblock); //debugging
	//printf("getting group descriptor block\n"); //debugging
	//get_block(dev, 2, buf);
	lseek1(dev, 2*BLKSIZE);
	read1(dev, buf, 1024);
	gp = (GD *)buf;
	imap = gp->bg_inode_bitmap;
	bmap =  gp->bg_block_bitmap;
	iblock =gp->bg_inode_table;
	//printf("from group descriptor: imap = %d, bmap = %d, iblock = %d\n", imap, bmap, iblock); //debugging

	//step 4
	//printf("step 4, getting ino of mount_point"); //debugging
	ino = getino(mount_point);
	if(ino == 0)
	{
		printf("%s doesn't exist, cannot mount\n", mount_point);
		return 0;
	}
	printf("ino of %s is: %d\n", mount_point, ino); //debugging
	mip = iget(fd, ino);
	//printf("got mip of %s\n", mount_point); //debugging

	//step 5
	//printf("step 5, checking if %s is a dir\n", mount_point); //debugging
	if(!S_ISDIR(mip->INODE.i_mode))
	{
		printf("%s is not a directory, cannot mount\n", mount_point);
	}
	//printf("%s is a dir\n", mount_point); //debugging

	//step 6
	//printf("step 6\n"); //debugging
	//printf("looking for empty mtable space\n"); //debugging
	index = 0;
	while(index < 4)
	{
		if(mtable[index] == NULL)
		{
			break;
		}
		index++;
	}

	if(index < 4)
	{
		//printf("found empty mtable space at index %d\n", index); //debugging
		mtable[index] = (MTABLE *)malloc(sizeof(MTABLE));
		mtable[index]->dev = dev;
		mtable[index]->nblock = nblock;
		mtable[index]->bmap = bmap;
		mtable[index]->imap = imap;
		mtable[index]->iblock = iblock;
		mtable[index] -> mountDirPtr = mip;
		strcpy(mtable[index]->deviceName, filesys);
		strcpy(mtable[index]->mountedDirName, mount_point);
	}
	else
	{
		printf("mount table is full\n");
		return 0;
	}

	//step 7
	//printf("step 7, make mip point to mtable[%d]\n", index); //debugging
	printf("mip about to change has ino = %d and dev = %d\n", mip -> ino, mip -> dev); //debugging
	mip -> mounted = 1;
	printf("mip->mounted = %d\n", mip -> mounted); //debugging
	mip -> mptr = mtable[index];
	mip -> dirty = 1;
	printf("mip -> dirty = %d\n", mip -> dirty); //debugging
	printf("in mount, marked mip with ino = %d as mounted\n", mip -> ino); //debugging
	iput(mip);
	updateMipOfOpenFile(filesys, mip);
	//printf("successfully mounted %s\n", filesys); //debugging
	return 1;
}
int umount1(char *filesys)
{
	/*
	1. Search the MOUNT table to check filesys is indeed mounted
	2. Check (by checking all active minode[].dev) whether any file is active in the mounted filesys; If so, reject
	3. Find the mount_point's in-memory inode, which should be in mememory while it's mounted on. Reset the minode's mounted flag to 0; then iput() the minode
	4. return success
	*/

	int index = 0;
	MINODE *mip;

	//step 1
	//printf("searching mount table to check if %s is mounted\n", filesys); //debugging
	while(index < 4)
	{
		if(mtable[index] != NULL)
		{
			if(strcmp(filesys, mtable[index]->deviceName) == 0)
			{
				break;
			}
		}
		index++;
	}

	if(index == 4)
	{
		printf("%s is not currently mounted so cannot unmount\n", filesys);
		return 0;
	}


	//step 2


	//step 3
	close1(mtable[index]->dev);
	mip = mtable[index] -> mountDirPtr;
	mip -> mounted = 0;
	mip -> mptr = NULL;
	mtable[index] = NULL;

	//step 4
	return 1;
}

int checkIfFileOpenForRead(char *filename)
{
	int ino, index = 0;
	MINODE *mip;
	OFT *oftp;

	ino = getino(filename);
	mip = iget(fd, ino);

	//printf("just before loop\n"); //debugging

	while(index < NFD)
	{
		//printf("in loop\n"); //debugging
		oftp = running -> fd[index];
		if(oftp != NULL)
		{
			//printf("oftp is not null\n"); //debugging
			if(oftp->mptr == mip)
			{
				//printf("oftp->mptr is equal to mip\n"); //debugging
				if(oftp -> mode == 0 || oftp -> mode == 2) //looks for only R and RW
				{
					printf("%s is open for R or RW\n", filename); //debugging
					iput(mip);
					return index; //return file descriptor of file
				}
			}
		}
		index++;
	}

	iput(mip);
	return -1;
}

int checkIfFileOpenForWrite(char *filename)
{
	int ino, index = 0;
	MINODE *mip;

	ino = getino(filename);
	mip = iget(fd, ino);

	while(index < NFD)
	{
		OFT *oftp = running -> fd[index];
		if(oftp != NULL)
		{
			if(oftp->mptr == mip)
			{
				if(oftp -> mode == 1) //only looks for W
				{
					iput(mip);
					return index; //return file descriptor of file
				}
			}
		}
		index++;
	}
	iput(mip);
	return -1;
}

int cat1(char *filename)
{
		//printf("Debugging: inside cat \n");
	int fileDesc = checkIfFileOpenForRead(filename);
	int isOpen = 1;
	int n=0,i=0;
	char buf[BLKSIZE+1];

	if(fileDesc == -1)
	{
		isOpen = 0;
		fileDesc = checkIfFileOpenForWrite(filename);
		if(fileDesc > -1)
		{
			printf("error: %s is open for write, not read\n", filename);
			return 0;
		}
		fileDesc = open1(filename,"R");
	}

	//printf("fileDesc is %d\n", fileDesc); //debugging

	//printf("Debugging: inside cat - before first read\n");
	//n = read1(fileDesc,buf,BLKSIZE);
	
	//getchar();
	//printf("Debugging: inside cat - before while loop , n = %d\n",n);
	while(n = read1(fileDesc,buf,1024))
	{
		
		//getchar();
		buf[n] = '\0';
		//printf("Debugging: inside cat - after getting buffer");
		printf("%s\n", buf);	

	}
	//printf("Debugging: inside cat - printing newline\n");
	printf("\n");
	if(!isOpen)
	{
		close1(fileDesc);
	}

	return 1;
}

int copy(char *oldFile, char *newFile)
{
		/*
	1. fd = open src for READ;
2. gd = open dst for W|CREAT; In the project, you may have to creat the dst file
                              first, then open it for WRITE, 
                              OR if open for WRITE fails, creat it and then open
                              it for WRITE.
3. while( n=read(fd, buf[ ], 1024) ){
       write(gd, buf, n);
	*/

	int fileDesc1 = checkIfFileOpenForRead(oldFile);
	int isOpen = 1;
	int fileDesc2,i,n;
	int creatNew = creat1(newFile);
	int buf[BLKSIZE];

	printf("fileDesc is %d for %s\n", fileDesc1, oldFile); //debugging

	if(fileDesc1 == -1)
	{
		isOpen = 0;
		fileDesc1 = checkIfFileOpenForWrite(oldFile);
		if(fileDesc1 > -1)
		{
			printf("error: %s is open for write, not read\n", oldFile); //debugging
			return 0;
		}
		fileDesc1 = open1(oldFile,"R");
	}
	// search and check if newfile already there


	if(!creatNew)// new file created
	{
		printf("Error: Could not copy file\n");
		return 0;
	}
		// open it for write
	fileDesc2 = open1(newFile,"W");	

	while(n = read1(fileDesc1,buf,BLKSIZE));
	{	
		write_file(fileDesc2,buf,BLKSIZE);
	}
	//printf("\n");
	if(!isOpen)
	{
		close1(fileDesc1);
	}
	close1(fileDesc2);

	return 1;
}

int move(char *oldFile, char *newFile)
{
	int dev = checkIfFileOpenForRead(oldFile);
	MINODE *mip;

	if(dev == -1)
	{
		dev = checkIfFileOpenForWrite(oldFile);
		if(dev > -1)
		{
			printf("error: %s is open for write, not read\n"); //debugging
			return 0;
		}
		dev = open1(oldFile, "R");
	}

	if(dev < 0)
	{
		return 0;
	}

	//printf("dev is: %d\n", dev); //debugging

	mip = running->fd[dev]->mptr;
	if(mip->dev == dev) //same file
	{
		//printf("moving to same file\n"); //debugging
		link1(oldFile, newFile);
		unlink1(oldFile);
	}
	else //different files
	{
		close1(dev);
		//printf("calling copy\n"); //debugging
		copy(oldFile, newFile);
		//printf("calling unlink\n"); //debugging
		unlink1(oldFile);
		//printf("done with the move\n"); //debugging
	}

	return 1;
}

int printOpenFiles()
{
	int index = 0;
	OFT *oftp;
	printf("currently opened fds with corresponding modes:\n");
	printf("name\tfd\tmode\n");
	while(index < NFD)
	{
		oftp = running -> fd[index];
		if(oftp != NULL)
		{
			printf("%s\t%d\t%d\n", oftp->filename, index, oftp->mode);
		}
		index++;
	}
}

int read1(int fd,char buf[],int nbytes)
{
	OFT* oftp ;//= malloc(sizeof(OFT));
	INODE*ip = malloc(sizeof(INODE));
	int device,fileSize,remain,blk;
	int count =0;
	int i=0;
	char *cp;
	char m_buf[BLKSIZE];
	char indirectBuf[256];
	int *indir_Buf ,*dblindirectBuf;
	int dblBlk;

	INODE inode;

	oftp = running->fd[fd];
	inode = oftp->mptr->INODE;

	if(inode.i_uid != running -> uid)
	{
		printf("user does not own this file\n");
		return 0;
	}
	if((inode.i_mode & 1 << 8) == 0)
	{
		printf("user does not have permission to read this file\n");
		return 0;
	}
	

	//step 2
	//int count = 0;
	if(fd < 0 || fd > 15)
	{
		printf("not a valid file descriptor\n");
		return 0;
	}

	if(running->fd[fd]==NULL)
	{
		printf("not a valid file descriptor\n");
		return -1;
	}
	if((running->fd[fd]->mode!=0)&&(running->fd[fd]->mode!=2))
	{
		printf("File not open for read\n");
		return -1;
	}
	//printf("Debugging: inside read1, nbytes = %d\n");
	//oftp = running->fd[fd];
	mip = running->fd[fd]->mptr;//oftp->mptr;
	ip = &mip->INODE;
	device = mip->dev;
	fileSize = ip->i_size;
	remain = fileSize - running->fd[fd]->offset;
	int offset =running->fd[fd]->offset;
	//printf("Offset = %d\n",offset);
	//printf("filesize = %d\n",fileSize);
	//printf("remain = %d\n",remain);
	//getchar();

	while((nbytes>0)&&(remain>0))
	{
		
		//printf("debugging: inside remain =0 loop\n");
		int lblk =  running->fd[fd]->offset/BLKSIZE;
		int start = running->fd[fd]->offset % BLKSIZE;

		//get the realblk into buf
		//if no indirect
		if(lblk <12)
		{
			blk = ip->i_block[lblk];

		}
		//first indirect
		else if((lblk>=12)&&(lblk <256+12))
		{
			get_block(device,ip->i_block[12],indirectBuf);
			indir_Buf = (int*)indirectBuf;
			blk = *(indir_Buf + (lblk - 12));
		}
		//double indirect
		else
		{
			get_block(device,ip->i_block[13],indirectBuf); // get the double indirect block into memory
			dblindirectBuf = (int*)indirectBuf; // cast it to an integer array
			dblBlk = *(dblindirectBuf + (lblk-268)/256); // use mailman's algorithm to find the blk that contains the linear address of the logical block
			get_block(device,dblBlk,indirectBuf); // read that block into memory
			dblindirectBuf = (int*)indirectBuf; // cast it into an int array
			blk = *(dblindirectBuf+(lblk - 268)%256); // identify the offset of the require logical block within the block containing it using the second part of mailman's algorithm

		}
		//REMOVE COMMENT IF indirect doesn't work : blk = running->fd[fd]->mptr->INODE.i_block[lblk];
		//add logic for indirect blocks
		get_block(device,blk,m_buf);
		cp = m_buf + start;
		int space = BLKSIZE - start; // space in buf

		if(remain < space)
		{
			space = remain;
		}
		int chunk;
		if(nbytes < space)
		{
			chunk = nbytes;
		}
		else
		{
			chunk = space;
		}

		memcpy(buf,cp,chunk);
		//printf("buf is %s\n", buf); //debugging
		count+=chunk;
		running->fd[fd]->offset += chunk;
		//oftp = running->fd[fd];
		remain-=chunk;
		nbytes-=chunk;


	}
	//printf("count returning = %d\n",count);
	//getchar();
	return count;


}

int getMode(char*parameter)
{	

	if(strcmp("R",parameter)==0)
	{

		return 0;
	}

	if(strcmp("W",parameter)==0)
	{
		return 1;
	}

	if(strcmp("RW",parameter)==0)
	{
		return 2;
	}
	if(strcmp("APPEND",parameter)==0)
	{
		return 3;
	}
	return -1;
}
int open1(char pathname[],char*parameter)
{

	/*
	1. ask for a pathname and mode to open:
         You may use mode = 0|1|2|3 for R|W|RW|APPEND
	*/
	int mode = getMode(parameter);
	int j;

	/*
	2. get pathname's inumber:
         ino = getino(&dev, pathname);
	*/
	int ino = getino(pathname);
	int i=0;

	//printf("Debugging:In open- got ino of pathname\n");
	if(mode==-1)
	{
		printf("BAD MODE\n");
		return -1;
	}

	mip = iget(fd,ino);
	//printf("Debugging:In open- after iget\n");
	//INODE*ip = &mip->INODE;
	if(!mip)
	{
		printf("File does not exist\n"); 
		return -1;
	}

	if(!(S_ISREG(mip->INODE.i_mode)))
	{
		printf("File is not a regular file - cannot open\n");
		iput(mip);
		return -1;
	}

	OFT *oftp = malloc(sizeof(OFT));
	//printf("Debugging:In open- after OFTP malloc\n");

 	for(i=0;i<15;i++)
 	{
 		//oftp=running->fd[i];
 		//check if already open and incompatible type
 		if(running->fd[i])
 		{
 			oftp = running->fd[i];
 			if(oftp->mptr==mip)
 			{
 				printf("Error: File already open for R|RW|W|APPEND\n");
 				iput(mip);
 				return -1;
 			}
 		}

 	}
	//check if proc has open filedescriptor available

	oftp = NULL;

	oftp = malloc(sizeof(OFT));

 	for(j=0;j<15;j++)
 	{
 		if(running->fd[j]==0)
 		{
 			break;
 		}
 		if(j== 15-1)
 		{
 			printf("Proc has reached file limit- cannot open\n");
 			iput(mip);
 			return -1;
 		}
 	}
 	oftp->filename = (char *)malloc(strlen(pathname)*sizeof(char));
 	strcpy(oftp->filename, pathname);
 	oftp->mode=mode;
 	oftp->refCount=1;
 	oftp->mptr =mip;
 	switch(oftp->mode)
 	{
 		case 0: oftp->offset =0 ; break;
 		case 1: oftp->offset =0 ; break;
 		case 2: oftp->offset =0 ; break;
 		case 3: oftp->offset =mip->INODE.i_size ; break;
 	}
 	running->fd[j]=oftp;
 	//
 	ip->i_atime = time(0L);
 	mip->dirty = 1;

 	printf("fd of file is %d\n",j);

 	return j;

}

int write1(int fd)
{
	if(fd < 0 || fd > 15)
	{
		printf("not a valid file descriptor\n");
		return 0;
	}
	if(running->fd[fd] != 0)
	{
		OFT *optr;
		INODE inode;
		optr = running->fd[fd];
		inode = optr->mptr->INODE;
		char string[1024];
		int nbytes;
		if(optr -> mode == 0)
		{
			printf("file is open for read, not write\n");
			return 0;
		}
		if(inode.i_uid != running->uid)
		{
			printf("user does not own this file\n");
			return 0;
		}
		//printf((inode.i_mode & 1 << 7) ? "w\n" : "-\n"); //debugging
		if((inode.i_mode & 1 << 7) == 0)
		{
			printf("user does not have write permissions on this file\n");
			return 0;
		}
		//printf("%d\n",(inode.i_mode & 1 << 4)); //debugging
		printf("Input the string to write to the file: ");
		fgets(string, 1024, stdin);
		string[strlen(string) - 1] = 0;
		//printf("string is: %s\n", string); //debugging
		nbytes = strlen(string);
		return write_file(fd, string, nbytes);
	}
	else
	{
		printf("fd is not a currently opened file\n");
		return 0;
	}
}

int lseek1(int fd, int offset) //changes file offset to new file offset, returns old file offset
{
	if(fd < 0 || fd > 15) //invalid fd
	{
		printf("not a valid file descriptor\n");
		return 0;
	}
	if(running->fd[fd] != 0)
	{
		OFT *oftp = running -> fd[fd];
		if(offset < 0)
		{
			printf("cannot lseek to a negative numbered offset\n");
			return 0;
		}
		int oldOffset = oftp->offset;
		oftp->offset = offset;
		return oldOffset;
	}
	else
	{
		printf("fd is not a currently opened file\n");
		return 0;
	}
}

int write_file(int dev, char *ubuf, int nbytes) 
{
	/*
	1. lock minode;
	2. count = 0; //number of bytes written
	3. while(nbytes) {
		compute logical block: lbk = oftp->offset/BLKSIZE;
		compute start byte: start = oftp->offset % BLKSIZE;
		4. convert lbk to physical block number, blk;
		5. read_block(dev, blk, kbuf); //read blk into kbuf[BLKSIZE];
		   char *cp = kbuf + start; remain = BLKSIZE - start;
		6. while(remain) //copy bytes from kbuf[] to ubuf[]
		{
			put_ubyte(*cp++, *ubuf++);
			offset++; count++; //inc offset; count;
			remain--; nbytes--; //dec remain, nbytes;
			if(offset > fileSize) fileSize++; //inc file size
			if(nbytes <= 0) break;
		}
		7. write_block(dev, blk, kbuf);
	}
	8. set minode dirty = 1; //mark minode dirty for iput()
	   unlock(minode);
	   return count;
	*/

	int lbk, start, remain;
	OFT *oftp;
	char *cp;
	int blk;
	int fileSize;
	int length;

	char indirectBuf[256];
	int *indir_Buf ,*dblindirectBuf;
	int dblBlk;

	//step 2
	int count = 0;

	bzero(buf, BLKSIZE);

	//printf("ubuf is: %s\n", ubuf); //debugging

	//step 3
	oftp = running->fd[dev];
	lbk = oftp->offset/BLKSIZE;
	//printf("logic block is: %d\n", lbk); //debugging
	start = oftp->offset % BLKSIZE;
	//printf("start is: %d\n", start); //debugging

	fileSize = oftp->mptr->INODE.i_size;
	//printf("fileSize is: %d\n", fileSize); //debugging

	//step 4
	//blk = oftp->mptr->INODE.i_block[lbk];
	//printf("blk is: %d\n", blk); //debugging
	//add logic for indirect blocks

		if(lbk <12)
		{
			blk = ip->i_block[lbk];

		}
		//first indirect
		else if((lbk>=12)&&(lbk <256+12))
		{
			get_block(dev,ip->i_block[12],indirectBuf);
			indir_Buf = (int*)indirectBuf;
			blk = *(indir_Buf + (lbk - 12));
		}
		//double indirect
		else
		{
			get_block(dev,ip->i_block[13],indirectBuf); // get the double indirect block into memory
			dblindirectBuf = (int*)indirectBuf; // cast it to an integer array
			dblBlk = *(dblindirectBuf + (lbk-12-256)/256); // use mailman's algorithm to find the blk that contains the linear address of the logical block
			get_block(dev,dblBlk,indirectBuf); // read that block into memory
			dblindirectBuf = (int*)indirectBuf; // cast it into an int array
			blk = *(dblindirectBuf+(lbk - 12-256)%256); // identify the offset of the require logical block within the block containing it using the second part of mailman's algorithm

		}

	//step 5
	get_block(fd, blk, buf);
	cp = buf + start;
	remain = BLKSIZE - start;
	//printf("remain is: %d\n"); //debugging

	//step 6
	
	length = strlen(ubuf);
	//printf("length is: %d\n", length); //debugging
	if(length < remain)
	{
		//printf("length < remain\n"); //debugging
		char temp[BLKSIZE];
		bzero(temp, BLKSIZE);
		//printf("temp is: %s\n", temp); //debugging
		if(start != 0)
		{
			strncpy(temp, buf, start);
			//printf("temp is now: %s\n", temp); //debugging
		}
		strcat(temp, ubuf);
		//printf("temp is now: %s\n", temp); //debugging
		strcpy(buf, temp);
		//printf("buf is now: %s\n", buf); //debugging
		fileSize = oftp->mptr->INODE.i_size = fileSize + length;
		//printf("filesize is now: %d\n", fileSize); //debugging
	}
	else
	{
		//printf("length >= remain\n"); //debugging
		char temp[BLKSIZE];
		bzero(temp, BLKSIZE);
		//printf("temp is %s\n", temp); //debugging
		if(start != 0)
		{
			strncpy(temp, buf, start);
			//printf("temp is now: %s\n", temp); //debugging
		}
		strncat(temp, ubuf, remain);
		//printf("temp is now: %s\n", temp); //debugging
		strcpy(buf, temp);
		//printf("buf is now: %s\n", buf); //debugging
		fileSize = oftp->mptr->INODE.i_size = fileSize + remain;
		//printf("filesize is now: %d\n", fileSize); //debugging
	}

	//step 7

	put_block(fd, blk, buf);
+
	//step 8
	//oftp->mptr->dirty = 1;
	iput(oftp->mptr);
	return count;
}

int close1(int fd) 
{
	/*
	1. check fd is a valid opened file descriptor
	2. if (PROC's fd[fd] != 0) {
		3. if(openTable's mode == READ/WRITE PIPE)
			return close_pipe(fd); //close pipe descriptor;
		4. if(--refCount == 0) {
			lock(minodeptr);
			iput(minode);
		}
	5. clear fd[fd] = 0; //clear fd[fd] to 0
	6. return success
	}
	*/

	//step 1
	if(fd < 0 || fd > 15) 
	{
		printf("not a valid file descriptor\n");
		return 0;
	}

	//step 2
	if(running->fd[fd] != 0) 
	{
		//not sure what to do for step 3

		OFT *oftp = running->fd[fd];

		//step 4
		int refCount = oftp->refCount;
		refCount--;
		if(refCount == 0)
		{
			//lock(oftp->mptr);
			iput(oftp->mptr);
		}
		else
		{
			printf("other processes are using this fd, cannot close\n");
			return 0;
		}
	}
	else 
	{
		printf("fd is not currently opened\n");
		return 0;
	}

	//step 5
	running->fd[fd] = 0;

	//step 6
	return 1;
}

int unlink1(char *filename)
{
	/*
	1. get filename's minode
		ino = getino(filename);
		mip = iget(dev, ino);
		check it's a REG or symbolic LNK file; cannot be a DIR
	2. remove name entry from parent DIR's data block
		parent = dirname(filename);
		child = basename(filename);
		pino = getino(parent);
		pimp = iget(dev, pino);
		rm_child(pmip, ino, child);
		pmip->dirty = 1;
		iput(pmip);
	3. decrement INODE's link_count by 1
		mip->INODE.i_links_count--;
	4. if(mip->INODE.i_links_count > 0)
			mip->dirty = 1; //for write INODE back to disk
	5. else { //if links_count = 0: remove filename
		deallocate all datablocks in INODE;
		deallocate INODE;
	}
	iput(mip); //release mip
	*/
	int ino, pino;
	MINODE *mip, *pmip;
	char *parent, *child;
	INODE inode;

	//step 1
	ino = getino(filename);
	mip = iget(fd, ino);
	inode = mip->INODE;

	if(S_ISDIR(mip -> INODE.i_mode))
	{
		return 0;
	}

	if(inode.i_uid != running->uid)
	{
		printf("user does not own this file\n");
		return 0;
	}
	if((inode.i_mode & 1 << 7) == 0)
	{
		printf("user does not have write permissions on this file\n");
		return 0;
	}

	//step 2
	parent = dirname(filename);
	child = basename(filename);
	pino = getino(parent);
	pmip = iget(fd, pino);
	rm_child(pmip, child);
	pmip->dirty = 1;
	iput(pmip);

	//step 3
	mip->INODE.i_links_count--;

	//step 4
	if(mip->INODE.i_links_count > 0)
	{
		mip->dirty = 1;
	}

	//step 5
	else
	{
		int index = 0;
		while(mip->INODE.i_block[index] && index < 12)
		{
			bdalloc(fd, mip->INODE.i_block[index]);
			index++;
		}
		idalloc(mip);
	}

	iput(mip);


	return 1;
}

int readlink1(char* file)
{
	//1. get file's INODE in memory; verify in's a LNK file
	//2. copy target filename from INODE.i_block[] into buffer
	//3. return file size

	int ino, sino;
	MINODE *mip, *smip;
	char *filename;
	int filesize;

	//step 1
	ino = getino(file);
	mip = iget(fd, ino);

	if(!S_ISLNK(mip->INODE.i_mode))
	{
		return -1;
	}

	//step 2
	filename = (char *)mip->INODE.i_block;
	//printf("filename linked to is: %s\n", filename); //debugging

	//step 3
	sino = getino(filename);
	smip = iget(fd, sino);
	filesize = mip->INODE.i_size;
	printf("filesize is: %d\n", filesize);

	iput(mip);
	iput(smip);

	return filesize;
}

int touch1(char *file)
{
	//change file's access time to current time
	int ino;
	MINODE *mip;
	INODE inode;

	ino = getino(file);

	if(!ino) //file doesn't exist so need to create (that's what the real touch command does)
	{
		return creat1(file);
	}

	mip = iget(fd, ino);

	inode = mip->INODE;

	if(inode.i_uid != running->uid)
	{
		printf("user does not own this file\n");
		return 0;
	}
	//printf((inode.i_mode & 1 << 7) ? "w\n" : "-\n"); //debugging
	if((inode.i_mode & 1 << 7) == 0)
	{
		printf("user does not have write permissions on this file\n");
		return 0;
	}

	mip->INODE.i_mtime = time(0L);

	mip-> dirty = 1;

	iput(mip);

	return 1;
}

int chmod1(char* pathname, char *oct)
{
	int ino;
	MINODE *mip;
	long int o;

	ino = getino(pathname);

	if(!ino)
	{
		return 0;
	}

	mip = iget(fd, ino);

	if(mip->INODE.i_uid != running->uid)
	{
		printf("user does not own this file so cannot change permissions\n");
		iput(mip);
		return 0;
	}

	o = strtol(oct, NULL, 8); //interpret oct as octal value

	mip->INODE.i_mode = (mip->INODE.i_mode & 0xF000) | o; //or o with old mode to set new mode

	//mip->INODE.i_mode = oct;

	mip -> dirty = 1;

	iput(mip);

	return 1;
}

int stat1(char *pathname, STAT *st)
{
	/*
	1. get INODE of pathname into a minode
	2. copy (dev, ino) of minode to (st_dev, st_ino) of the STAT structure in user space
	3. copy other fields of INODE to STAT structure in user space
	4. iput(minode)
	*/

	int ino;
	MINODE *mip;
	char *time;

	st = (STAT *)malloc(sizeof(STAT));

	//step 1
	ino = getino(pathname);

	if(!ino)
	{
		printf("file does not exist");
		return 0;
	}

	mip = iget(fd, ino);

	//step 2
	st->st_dev = fd;
	st->st_ino = ino;

	//step 3
	st->st_mode = mip->INODE.i_mode;
	st->st_nlink = mip->INODE.i_links_count;
	st->st_uid = mip->INODE.i_uid;
	st->st_gid = mip->INODE.i_gid;
	st->st_size = mip ->INODE.i_size;
	st->st_blksize = 1024;
	st->st_blocks = mip->INODE.i_blocks;
	st->st_atime = mip->INODE.i_atime;
	st->st_mtime = mip->INODE.i_mtime;
	st->st_ctime = mip->INODE.i_ctime;

	printf("File: %s\n", pathname);
	printf("Size: %d\t", (int)st->st_size);
	printf("Blocks: %d\t", (int)st->st_blocks);
	printf("Block size: %d\t", (int)st->st_blksize);
	if(S_ISREG(st->st_mode)) printf("regular file\n");
	if(S_ISDIR(st->st_mode)) printf("directory\n");
	if(S_ISLNK(st->st_mode)) printf("link file\n");
	printf("Device: %d\t", (int)st->st_dev);
	printf("Inode: %d\t", (int)st->st_ino);
	printf("Links: %d\n", (int)st->st_nlink);
	printf("Access: ");
	printf((st->st_mode & 1 << 8) ? "r" : "-");
	printf((st->st_mode & 1 << 7) ? "w" : "-");
	printf((st->st_mode & 1 << 6) ? "x" : "-");
	printf((st->st_mode & 1 << 5) ? "r" : "-");
	printf((st->st_mode & 1 << 4) ? "w" : "-");
	printf((st->st_mode & 1 << 3) ? "x" : "-");
	printf((st->st_mode & 1 << 2) ? "r" : "-");
	printf((st->st_mode & 1 << 1) ? "w" : "-");
	printf((st->st_mode & 1 << 0) ? "x" : "-");
	printf("\t");
	printf("Uid: %d\t", (int)st->st_uid);
	printf("Gid: %d\n", (int)st->st_gid);
	time = ctime(&st->st_atime);
	printf("Access: %s\n", time);
	time = ctime(&st->st_mtime);
	printf("Modify: %s\n", time);
	time = ctime(&st->st_ctime);
	printf("Change: %s\n", time);

	//step 4
	iput(mip);

	return 1;
}

int symlink1(char*oldfile,char*newlink)
{
	int oino,pino,cino,result;
	MINODE *omip,*pmip,*cmip;
	char*parent; 
	char*child;
	char* oldname;
	char*newPath;
	strcpy(newPath,newlink);
	

	//printf("Debugging: oldfile = %s, newfile = %s\n",oldfile,newlink);

	child = basename(newlink);
	parent = dirname(newlink);
	
	
	//printf("Debugging: parent = %s, child = %s\n",parent,child);
	
	getchar();
	oino = getino(oldfile);

	if(!oino)
	{
		return 0;
	}
	omip = iget(fd,oino);

	pino = getino(parent);
	pmip = iget(fd,pino);

	if(!search(pmip, child))
	{
		result = my_symlink(pmip, child,strlen(oldfile), oldfile);
		if(!result)
		{
			return 0;
		}
		//printf("Debugging : before getting child ino\n");

		cino = getino(newPath);
		cmip =iget(fd,cino);
		//printf("Debugging : before enter child for target iblock\n");
		//enter_child(cmip,cino,oldfile);
		cmip->INODE.i_size = strlen(oldfile);
		//strcpy((char*)cmip->INODE.i_block,oldfile);

		get_block(fd, cmip->INODE.i_block[0], buf);
		dp = (DIR*)buf;
		strncpy(dp->name, oldfile, strlen(oldfile));

		cmip->INODE.i_mode = 0xA1A4;
		cmip->dirty =1;
		iput(cmip);
		pmip->dirty=1;
		iput(pmip);
		omip->INODE.i_links_count++;
		omip->dirty =1;
		iput(omip);

	}
	else
	{
		printf("%s already exists in %s\n", child, parent);
		iput(omip);
		iput(pmip);
		return 0;
	}


}

int my_symlink(MINODE *pmip, char *childName,int oldfileSize, char *oldfile)
{
	int ino, blk, i = 1;
	MINODE *mip;
	char *cp;
	DIR *dp;

	//step 1
	ino = ialloc(fd);
	blk = balloc(fd);

	//step 2
	mip = iget(fd, ino);
	mip->INODE.i_mode=120000; //file type and permissions
	mip->INODE.i_uid = running->uid; //owner uid
	mip->INODE.i_gid = running->gid; //group id
	mip->INODE.i_size = oldfileSize; //files have size of 0
	mip->INODE.i_links_count = 2; //links count = 2 because of . and ..
	mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0L);
	mip->INODE.i_blocks = 2; //LINUX: Blocks count has one data block
	/*mip->INODE.i_block[0] = blk;

	while(i<15)
	{
		mip->INODE.i_block[i] = 0;
		i++;
	}*/
	memcpy(mip->INODE.i_block, oldfile, oldfileSize);
	mip -> dirty = 1;
	iput(mip);

	//step 4
	return enter_child(pmip, ino, childName);
}
int link1(char*oldfile,char*newlink)
{
	int oino,result;
	MINODE*omip,*pmip;
	char*temp = newlink;
	char*parent; 
	char*child; 

	child = basename(newlink);
	parent = dirname(newlink);
	

	//printf("Debugging:%s is child name\n",child);
	//printf("Debugging:%s is dir name\n",parent);
	//getchar();

	oino = getino(oldfile);
	omip = iget(fd,oino);

	if(!S_ISREG(omip->INODE.i_mode))
	{
		printf("%s is not a file\n", oldfile);
		return 0;
	}
	int pino = getino(parent);
	pmip = iget(fd,pino);

	if(!search(pmip, child))
	{
		//step 4
		result = enter_child(pmip,oino,child);
		if(!result)
		{
			printf("Could not enter child\n");
			iput(pmip);
			iput(omip);
			return 0;
		}

		int ino = getino(child);
		MINODE *mip = iget(fd, ino);
		mip->INODE.i_mode = 0xA1A4;
		
		pmip -> dirty = 1;
		iput(pmip);
		omip->INODE.i_links_count++;
		omip->dirty =1;
		iput(omip);

	}
	else
	{
		printf("%s already exists in %s\n", child, parent);
		iput(omip);
		iput(pmip);
		return 0;
	}

	return 1;
}




int checkEmpty(MINODE*mip)
{
	char buf[1024];
	char*cp;
	DIR*dp;
	int numdirs=0;

	get_block(mip->dev,mip->INODE.i_block[0],buf);
	dp = (DIR*)buf;
	cp = buf;

	while(cp < buf +1024)
	{
		//printf("Debugging: num dirs  = %d\n",numdirs);
		if(numdirs > 2)
		{
			printf("Directory not empty : cannot delete\n");
			return 0;
		}
		cp+=dp->rec_len;
		dp = (DIR*)cp;
		numdirs++;
	}

	return 1;
}

int rmdir1(char *pathname)
{
	int ino, pino;
	MINODE *mip, *pmip;
	char *name = basename(pathname);
	INODE inode;

	//1. get in-memory INODE of pathname
	ino = getino(pathname);
	//printf("ino: %d\n", ino); //debugging
	mip = iget(fd, ino);
	inode = mip->INODE;

	//2. verify INODE is a DIR by INODE.imode field
	if(!S_ISDIR(mip->INODE.i_mode))
	{
		printf("%s is not a directory\n", pathname);
		return 0;
	}
	if(inode.i_uid != running->uid)
	{
		printf("user does not own this directory\n");
		return 0;
	}
		//printf((inode.i_mode & 1 << 7) ? "w\n" : "-\n"); //debugging
	if((inode.i_mode & 1 << 7) == 0)
	{
		printf("user does not have write permissions on this directory\n");
		return 0;
	}
	//verify minode is not busy (refCount = 1)
	//printf("refCount is: %d\n", mip->refCount); //debugging
	if(mip->refCount > 1)
	{
		printf("%s has a refCount greater than 0\n", pathname);
		return 0;
	}
	//verify dir is empty (traverse data blocks for number of entries = 2)
	if (!checkEmpty(mip))
	{		
		return 0;
	}

	//3. get parent ino and inode
	//pino = findIno(); //get pino from .. entry in INODE.i_block[0]
	findpino(mip, &ino, &pino);
	//printf("pino is: %d\n", pino); //debugging
	pmip = iget(mip->dev, pino);

	//4. get name from parent DIR's data block
	//findname(pmip, ino, name); //find name from parent DIR
	findname(pmip, ino, name);
	//printf("name: %s\n", name); //debugging

	//5. remove name from parent directory
	//printf("callling rm_child\n"); //debugging
	rm_child(pmip, name);
	//printf("exited rm_child\n"); //debugging


	//6. deallocate its data blocks and inode
	//printf("step 6\n"); //debugging
	bdalloc(mip->dev, mip->INODE.i_block[0]);
	idalloc(mip);
	iput(mip);

	//7. dec parent links_count by 1; mark parent pimp dirty;
	//printf("step 7\n"); //debugging
	pmip->INODE.i_links_count--;
	pmip->dirty = 1;
	iput(pmip);
}

int rm_child(MINODE *pmip, char *name)
{
	int ino;
	int datablk, offset;
	MINODE *mip;
	DIR *dp;

	/*//1. search parent INODE's data block(s) for the entry of name
	ino = search(pmip, name);
	if(ino == 0)
	{
		printf("%s doesn't exist \n", name);
		return 0;
	}
	dp = findDir(pmip, ino, &datablk);
	if (dp->rec_len == BLKSIZE)
	{
		bdalloc(fd, datablk);
		pmip -> inode.i_size -= BLKSIZE;

	}
	else if()
	{

	}*/
	//getchar(); // debugging
	char buff[1024];
	//printf("in rm_child\n"); //debugging
	get_block(fd,pmip->INODE.i_block[0],buff);
	char *cp = buff;
	dp = (DIR*)buff;
	char*endcp = buff;
	int sizeDel;
	char*prev;
	int size=0,i=0;
	int tmp;

	//getchar(); //debugging
	//printf("Debugging: in rm_child - before the while loop\n");
	// to get the end char pointer - move to end of buff
	while(endcp + dp->rec_len < buff +1024)
	{
		endcp +=dp->rec_len;
		dp = (DIR*)endcp;
		char temp[dp->name_len];
		memcpy(temp, dp->name, dp->name_len+1);
		temp[dp->name_len] = '\0';	
		//printf("Debugging : %s is the last entry\n",temp);
	}
	//reset dp to beginning
	dp = (DIR*)cp;
	//getchar(); // debugging
	while(cp < buff+1024)
	{	
		//getchar();
		char temp[dp->name_len];
		memcpy(temp, dp->name, dp->name_len+1);
		temp[dp->name_len] = '\0';	
		if((dp->name_len == strlen(name))&&(strcmp(temp, name) == 0))
		{
			tmp = dp->rec_len;
			//we found the dir to delete
			
			// if it is the last entry
			//store its size
			//sizeDel = dp->rec_len;
			//printf("Debugging: in rm_child - we found the dir %s with size %d\n",dp->name,dp->rec_len);
			//printf("dp address: %d\n", dp); //debugging
			if(cp == endcp)
			{
				//printf("Debugging: in rm_child - dir %s is the last dir\n",dp->name);

				dp = (DIR*)prev; // point to the previous entry
				//printf("Debugging: in rm_child - dir %s is the prev dir , current size = %d\n",dp->name,dp->rec_len);
				dp->rec_len += tmp; // add the size of deleted to prev
				//printf("Debugging: in rm_child - after delete , prev dir current size = %d\n",dp->rec_len);
				break;
			}
			else if(dp->rec_len == BLKSIZE) // it is first and only 
			{
				//printf("Debugging: in rm_child - dir %s is the only dir\n",dp->name);
				pmip -> INODE.i_size -= BLKSIZE;
				break;
			}
			else
			{
				//DIR *tp;
				//prev += dp->rec_len;
				dp = (DIR*)endcp;
				dp->rec_len+=tmp;
				//printf("Debugging: in rm_child - dir is middle or not only one\n");
				//cp+=dp->rec_len;
				//size = (&buff[1024]) - (cp+tp->rec_len);
				//printf("Debugging : dp = %d cp = %d size = %d\n ",(int)dp,(int)cp,size);
				memcpy(cp,cp+tmp,1024-i-tmp);

				//printf("Debugging : after memcpy\n");
				break;

			}

		}
		//move to next
		//printf(" = %d\n");// debugging

		prev = (int)cp;
		i+=dp->rec_len;
		cp+=dp->rec_len;
		dp=(DIR*)cp;

	}
	put_block(fd,pmip->INODE.i_block[0],buff);
	return 0;
}
int creat1(char *pathname)
{
	char *parent, *child;
	int ino;
	int result;
	MINODE *pmip;
	INODE inode;

	//step 1
	parent = dirname(pathname);
	child = basename(pathname);
	ino = getino(parent); //get parent's ino

	//step 2
	if(ino == 0)
	{
		printf("%s does not exist\n", parent);
		return 0;
	}
	pmip = iget(fd, ino); //get parent MINODE
	inode = pmip->INODE;
	if(!S_ISDIR(pmip->INODE.i_mode))
	{
			printf("%s is not a directory\n", parent);
			return 0;
	}
	if(inode.i_uid != running->uid)
	{
		printf("user does not own this directory\n");
		return 0;
	}
	//printf((inode.i_mode & 1 << 7) ? "w\n" : "-\n"); //debugging
	if((inode.i_mode & 1 << 7) == 0)
	{
		printf("user does not have write permissions on this directory\n");
		return 0;
	}

	//step 3
	if(!search(pmip, child))
	{
		//step 4
		result = my_creat(pmip, child);
		if(!result)
		{
			return 0;
		}
		//step 5
		//removed incrementing parent link count because this is a file, not directory
		pmip -> dirty = 1;
		iput(pmip);
	}
	else
	{
		printf("%s already exists in %s\n", child, parent);
		iput(pmip);
		return 0;
	}
}

int my_creat(MINODE *pmip, char *childName)
{
	int ino, blk, i = 1;
	MINODE *mip;
	char *cp;
	DIR *dp;

	//step 1
	ino = ialloc(fd);
	blk = balloc(fd);

	//step 2
	mip = iget(fd, ino);
	mip->INODE.i_mode=0x81A4; //file type and permissions
	mip->INODE.i_uid = running->uid; //owner uid
	mip->INODE.i_gid = running->gid; //group id
	mip->INODE.i_size = 0; //files have size of 0
	mip->INODE.i_links_count = 2; //links count = 2 because of . and ..
	mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0L);
	mip->INODE.i_blocks = 2; //LINUX: Blocks count has one data block
	mip->INODE.i_block[0] = blk;

	while(i<15)
	{
		mip->INODE.i_block[i] = 0;
		i++;
	}
	mip -> dirty = 1;
	iput(mip);

	// //step 3
	// memset(buf, 0, 1024);
	// dp = (DIR *)buf;
	// dp->inode = ino;
	// strncpy(dp->name, ".", 1);
	// dp->name_len = 1;
	// dp->rec_len = 12;
	// cp = buf;
	// cp += dp->rec_len;
	// dp = (DIR *)cp;
	// dp->inode = pmip->ino;
	// dp->name_len = 2;
	// strncpy(dp->name, "..", 2);
	// dp->rec_len = BLKSIZE - 12;
	// put_block(fd, blk, buf);

	//step 4
	return enter_child(pmip, ino, childName);
}
int mkdir1(char *pathname)
{
	/*
	1. Use library functions dirname(pathname), basename(pathname) to divide pathname
	into parent and child, e.g. pathname = /a/b/c then parent=/a/b; child=c
	2. parent must exist and is a DIR
	3. child must not exist in parent DIR; search(pmip, child) must return 0;
	4. call my_mkdir(pmip, child) to create a DIR;
	5. increment parent INODE's links_count by 1 and mark pmip dirty; iput(pmip);
	*/

	//printf("in mkdir1\n"); //debugging
	char *parent, *child;
	int ino;
	int result;
	MINODE *pmip;
	INODE inode;

	//step 1
	parent = dirname(pathname);
	child = basename(pathname);
	ino = getino(parent); //get parent's ino

	//printf("parent is: %s\n", parent); //debugging
	//printf("child is: %s\n", child); //debugging
	//printf("ino of parent is: %d\n", ino); //debugging

	//step 2
	if(ino == 0)
	{
		printf("%s does not exist\n", parent);
		return 0;
	}
	pmip = iget(fd, ino); //get parent MINODE
	inode = pmip->INODE;
	if(!S_ISDIR(pmip->INODE.i_mode))
	{
			printf("%s is not a directory\n", parent);
			return 0;
	}

	if(inode.i_uid != running->uid)
	{
		printf("user does not own this directory\n");
		return 0;
	}
	//printf((inode.i_mode & 1 << 7) ? "w\n" : "-\n"); //debugging
	if((inode.i_mode & 1 << 7) == 0)
	{
		printf("user does not have write permissions on this directory\n");
		return 0;
	}

	//step 3
	//printf("calling search\n"); //debugging
	if(!search(pmip, child))
	{
		//step 4
		//printf("child does not exist so calling my_mkdir\n"); //debugging
		result = my_mkdir(pmip, child);
		if(!result)
		{
			printf("failed to create directory\n");
			return 0;
		}
		//step 5
		pmip -> INODE.i_links_count++;
		pmip -> dirty = 1;
		iput(pmip);
	}
	else
	{
		printf("%s already exists in %s\n", child, parent);
		iput(pmip);
		return 0;
	}
	//printf("leaving mkdir1\n"); //debugging
	return 1;
}

int my_mkdir(MINODE *pmip, char *childName)
{
	/*
	1. Allocate an INODE and a disk block:
		ino = ialloc(fd);
		blk = balloc(fd);
	2. mip = iget(fd, ino) //load INODE into a minode
		initialize mip->INODE as a DIR INODE
		mip->INODE.i_block[0] = blk; other i_block[] = 0;
		mark minode modified (dirty);
		iput(mip); //write INODE back to disk
	3. make data block 0 of INODE to contain . and .. entries;
		write to disk block blk
	4. enter_child(pmip, ino, childName); which enters (ino, childName) as a dir_entry
	to the parent INODE
	*/
	int ino, blk, i = 1;
	MINODE *mip;
	char *cp;
	DIR *dp;

	//printf("in my_mkdir\n"); //debugging

	//step 1
	ino = ialloc(fd);
	//printf("ino from ialloc is: %d\n", ino); //debugging
	blk = balloc(fd);
	//printf("blk from balloc is: %d\n", blk); //debugging

	//step 2
	//printf("setting up mip\n"); //debugging
	mip = iget(fd, ino);
	mip->INODE.i_mode=0x41ED; //DIR type and permissions
	mip->INODE.i_uid = running->uid; //owner uid
	mip->INODE.i_gid = running->gid; //group id
	mip->INODE.i_size = BLKSIZE; //size in bytes
	mip->INODE.i_links_count = 2; //links count = 2 because of . and ..
	mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0L);
	mip->INODE.i_blocks = 2; //LINUX: Blocks count has one data block
	mip->INODE.i_block[0] = blk;

	while(i<15)
	{
		mip->INODE.i_block[i] = 0;
		i++;
	}
	mip -> dirty = 1;
	iput(mip);

	//printf("done setting up mip\n"); //debugging

	//printf("setting up new directory with . and .. entries\n"); //debugging

	//step 3
	memset(buf, 0, 1024);
	dp = (DIR *)buf;
	dp->inode = ino;
	strncpy(dp->name, ".", 1);
	dp->name_len = 1;
	dp->rec_len = 12;
	cp = buf;
	cp += dp->rec_len;
	dp = (DIR *)cp;
	dp->inode = pmip->ino;
	dp->name_len = 2;
	strncpy(dp->name, "..", 2);
	dp->rec_len = BLKSIZE - 12;
	put_block(fd, blk, buf);

	//printf("done setting up . and ..\n"); //debugging

	//step 4
	//printf("calling enter child\n"); //debugging
	return enter_child(pmip, ino, childName);
}

int enter_child(MINODE *pmip, int ino, char *childName)
{
	//enters (ino, child) as a dir_entry to the parent INODE
	/*
	For each data block of parent DIR do
		1. Get parent's data block into a buf[]
		2. In a data block of the parent directory, each dir entry has an ideal length
			ideal_length=4*[(8+name_length +3)/4] which is a multiple of 4
			All dir entries rec_len = ideal_length, except the last entry. The rec_length
			of the LAST entry is to the end of the block, which may be larger than its 
			ideal_length
		3. In order to enter a new entry of child with n_len, the needed length is:
			need_length = 4*[(8+n_len+3)/4] which is a multiple of 4
		4. Step to the last entry in the data block:
			get_block(parent->dev, parent->INODE.i_block[i], buf);
			dp = (DIR *)buf;
			cp = buf;
			while(cp + dp->rec_len < buf + BLKSIZE)
			{
				cp+= dp->rec_len;
				dp = (DIR *)cp;
			}
			dp now points at last entry in block
			remain = LAST entry's rec_len - its ideal_length
			if(remain >= need_length)
			{
				enter the new entry as the LAST entry and trim the previous entry
				rec_len to its ideal_length
			}
			goto step 6
		5. If no space in existing data block(s):
			Allocate a new data block; increment parent size by BLKSIZE
			Enter new entry as the first entry in the new data block with rec_len = BLKSIZE
		6. Write data block back to disk
	*/
	int i, found = 0;
	int ideal_length, need_length, length, remain;
	get_block(fd, pmip->INODE.i_block[0], buf);
	char *cp;
	DIR *dp;
	for(i = 0; i < 12; i++)
	{
		cp = buf;
		dp = (DIR *)buf;
		//Step 1
		get_block(fd, pmip->INODE.i_block[i], buf);
		while(cp < buf + 1024)
		{
			//Step 2
			ideal_length = 4*((8+dp->name_len +3)/4);
			if(dp->rec_len > ideal_length)
			{
				found = 1;
				break; //last entry was found
			}
			cp+=dp->rec_len;
			dp = (DIR *)cp;
		}
		if(found == 1) //last entry was found
		{
			break;
		}
	}
	//Step 3
	length = strlen(childName);
	need_length = 4*((8+length+3)/4);
	//Step 4
	get_block(fd, pmip->INODE.i_block[i], buf);
	dp = (DIR *)buf;
	cp = buf;
	while(cp + dp->rec_len < buf + BLKSIZE)
	{
		cp+= dp->rec_len;
		dp=(DIR *)cp;
	}
	ideal_length = 4*((8+dp->name_len+3)/4);
	remain = (dp->rec_len) - ideal_length;
	if(remain >= need_length)
	{
		dp->rec_len = ideal_length;
		dp = (char *)dp + ideal_length;
		dp->inode = ino;
		dp->name_len = length;
		dp->rec_len = remain;
		strncpy(dp->name, childName, dp->name_len);
	}
	else //Step 5
	{
		int blk;
		i++; //increment i to access next block
		if(pmip->INODE.i_block[i] != 0)
		{
			return 0;
		}
		else
		{
			pmip->INODE.i_block[i] = balloc(fd);
			bzero(buf, BLKSIZE);
			dp = (DIR *)buf;
			dp->inode = ino;
			dp-> rec_len = BLKSIZE;
			dp->name_len = length;
			strncpy(dp->name, childName, dp->name_len); 
		}
	}
	//Step 6
	put_block(fd, pmip->INODE.i_block[i], buf);
	return 1;
}

int incFreeInodes(int dev)
{
	char buf[BLKSIZE];
	//inc free inodes count in SUPER and GD
	get_block(dev, 1, buf);
	sp=(SUPER *)buf;
	sp->s_free_inodes_count++;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp=(GD *)buf;
	gp->bg_free_inodes_count++;
	put_block(dev, 2, buf);
}

int incFreeBlocks(int dev)
{
	char buf[BLKSIZE];
	//inc free inodes count in SUPER and GD
	get_block(dev, 1, buf);
	sp=(SUPER *)buf;
	sp->s_free_blocks_count++;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp=(GD *)buf;
	gp->bg_free_blocks_count++;
	put_block(dev, 2, buf);
}

int idalloc(MINODE *mip)
{
	int i;
	char buf[BLKSIZE];
	int ino = mip->ino;
	if(ino > inode_count)
	{
		printf("inumber %d out of range\n", ino);
		return;
	}
	//get inode bitmap
	get_block(fd, inode_bitmap, buf);
	clr_bit(buf, ino-1);
	//write buf back
	put_block(fd, inode_bitmap, buf);
	//update free inode count in SUPER and GD
	incFreeInodes(fd);
}

int bdalloc(int dev, int datablk)
{
	int i;
	char buf[BLKSIZE];
	if(datablk > block_count)
	{
		printf("datablk %d out of range\n", datablk);
		return;
	}
	//get block bitmap
	get_block(fd, block_bitmap, buf);
	clr_bit(buf, datablk-1);
	//write buf back
	put_block(fd, block_bitmap, buf);
	//update free block count in SUPER and GD
	incFreeBlocks(dev);
}

int ialloc(int dev)
{
	int i;
	char buf[BLKSIZE];
	//assume imap, bmap are globals from superblock and GD
	get_block(dev, inode_bitmap, buf);
	for(i=0; i < inode_count; i++)
	{
		if(tst_bit(buf, i) == 0)
		{
			set_bit(buf, i);
			put_block(dev, inode_bitmap, buf);
			//update free inode count in SUPER and GD
			decFreeInodes(dev);
			return (i + 1);
		}
	}
	return 0; //out of FREE inodes
}

int balloc(int dev)
{
	int i;
	char buf[BLKSIZE];
	get_block(dev, block_bitmap, buf);
	for(i=0; i < block_count; i++)
	{
		if(tst_bit(buf, i) == 0)
		{
			set_bit(buf, i);
			put_block(dev, block_bitmap, buf);
			decFreeBlocks(dev);
			return (i + 1);
		}
	}
	return 0; //out of FREE blocks
}

int decFreeBlocks(int dev)
{
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_blocks_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD *)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, 2, buf);
}

int decFreeInodes(int dev)
{
	//dec free inodes count in SUPER and GD
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_inodes_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf);
	gp = (GD *)buf;
	gp->bg_free_inodes_count--;
	put_block(dev, 2, buf);
}

int clr_bit(char buf[], int bit)
{
	return buf[bit/8] &= ~(1 << (bit%8));
}



int tst_bit(char *buf, int bit)
{
	return buf[bit/8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit)
{
	return buf[bit/8] |= (1 << (bit % 8));
}


int get_block(int fd, int blk, char *buf)
{
	 lseek(fd, blk*BLKSIZE, SEEK_SET);
	 return read(fd, buf, BLKSIZE);
}

int put_block(int fd, int blk , char*buf)
{
	lseek(fd, blk*BLKSIZE, SEEK_SET);
	return write(fd,buf,BLKSIZE);
}

void init()
{
	int i;
	//printf("Debugging: in init - before proc\n");
	//proc[0] = malloc(sizeof(PROC));
	proc[0].uid =0;
	proc[0].cwd =0;
	//proc[1] = malloc(sizeof(PROC));
	proc[1].uid =1;
	proc[1].cwd =0;

	//running = malloc(sizeof(PROC));
	//running = &proc[0];
	//printf("Debugging: in init - before minode init\n");
	for (i=0;i<NMINODE;i++)
	{
		minodes[i] = malloc(sizeof(MINODE));
		minodes[i]->refCount =0;		
	}
	//printf("Debugging: in init - before root malloc\n");
	root = malloc(sizeof(MINODE));
	root =0;

	//printf("Debugging: in init - before running malloc\n");
	running =  malloc(sizeof(PROC));
	running = &proc[0];


}

MINODE *iget(int dev,int ino)
{
	//printf("Debugging: in iget - before malloc mip\n");
	//return : a pointer to an inode in memory
	MINODE *mi = malloc(sizeof(MINODE));
	ip = malloc(sizeof(INODE));
	//inode disk block
	int seek_pos = (ino-1)/8 + INODE_START;
	//inside the block
	int offset = (ino-1)%8;
	//char buf[1024];
	int i=0;

	while(i<NMINODE)
	{
		//printf("Debugging: in iget - before checking for existing inode\n");
		if((minodes[i]->dev == dev)&&(minodes[i]->ino==ino))
		{
			mi = minodes[i];
			mi->refCount ++;
			return mi;
		}
		i++;
	}

	//else
	//printf("Debugging: in iget - before checking for free inode slot\n");
	i=0;
	while((i<NMINODE)&&(minodes[i]->refCount!=0))
	{
		i++;
	}
	if(i>=NMINODE)
	{
		printf("Too many inodes in memory\n");
		exit(2);
	}
	//printf("Debugging: in iget - found inode slot at i= %d\n",i);
	minodes[i]->refCount =1;
	minodes[i]->dev = dev;
	minodes[i]->ino = ino;
	minodes[i]->dirty = 0;
	minodes[i]->mounted =0;
	minodes[i]->mptr =0;

	//printf("Debugging: in iget - before setting mip to address of minode[i]\n");
	mi = minodes[i];

	//printf("Debugging: in iget - before getting inode block\n");
	get_block(dev,seek_pos,buf);

	//printf("Debugging: in iget - before getting to inode within block\n");
	ip = (INODE*)buf + offset;

	//printf("Debugging: in iget - before setting *ip\n");
	mi->INODE = *ip;

	return mi;

}

int iput(MINODE *mip)
{
	//int ino = getino(".");
	int seek_pos = (mip->ino-1)/8 + INODE_START;
	int offset = (mip->ino-1)%8;

	mip->refCount--;

	if(mip->refCount >0) // still in use
	{
		return;
	}
	if(!mip->dirty) // same as version on disk
	{
		return;
	}

	printf("mip with ino = %d is dirty\n", mip->ino); //debugging

	//write it back to disk
	printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino);
	if(mip -> dev != fd)
	{
		lseek1(mip->dev, seek_pos*BLKSIZE);
	 	read1(mip->dev, buf, BLKSIZE);
	}
	else
	{
		get_block(mip->dev,seek_pos,buf);
	}

	ip = (INODE*)buf + offset;
	*ip = mip->INODE;

	if(mip -> dev != fd)
	{
		lseek1(mip->dev, seek_pos*BLKSIZE);
		write_file(mip->dev,buf,BLKSIZE);
	}
	else
	{
		put_block(mip->dev,seek_pos,buf);
	}
}

int search(MINODE *mip , char *name)
{
	int blk;
	char *cp;
	char temp[256];

	//printf("Debugging: in search - name =%s\n",name);
	*ip = mip->INODE;
	//search for the name in the data blocks of this inode

	blk = ip->i_block[0];
	get_block(fd,blk,buf);
	dp = (DIR*)buf;
	cp = buf;

	while(cp < buf+BLKSIZE)
	{
		strncpy(temp,dp->name,dp->name_len);
	 	temp[dp->name_len] = '\0';
	 	//printf("%s\n",dp->inode,dp->rec_len,dp->name_len,temp);
	 	//printf("Debugging: in search - temp =%s\n",temp);
	 	if(strcmp(temp,name)==0)
	 	{
	 		//printf("Debugging: strcmp success - temp =%s\n",temp);
	 		//printf("Debugging: in search - inode found =%d\n",dp->inode);
	 		return dp->inode;
	 	}
	 	memset(temp, 0, 256);
	 	cp+=dp->rec_len;
	 	dp =(DIR*)cp;

	}

return 0;

}

int tokenizer(char *string, char **outArray[64], char *delim)
{
	int n = 0;
	char *temp, *backup;
	backup = malloc(sizeof(char) * strlen(string));
	strcpy(backup, string);
	temp = strtok(backup, delim);
	while (temp != NULL)
	{
		//printf("Debugging: in tokenizer - temp =%s\n",temp);
		outArray[n] = temp;
		//printf("Debugging: in tokenizer - outArray[i] value = %s\n",outArray[n]);
		outArray[n+1] = 0;
		n++;
		temp = strtok(0, delim);
	}
	if ((n != 0)&&(n!=1))
		n--;
	return n;
}

int getino(char * pathName)
{
	int i,ino,seek_pos,offset;
	int n;
	int dev;

	
	 printf("getino: pathname=%s\n", pathName);
	 //getchar();
	 if (strcmp(pathName, "/")==0)
     	return 2; // asking for the root


 	if (pathName[0]=='/')
 	{
     mip = iget(fd, 2);
 	}
  	else
  	{
     mip = iget(fd, running->cwd->ino);
  	}

  	strcpy(buf,pathName);
  	n= tokenizer(pathName,&path,"/");
  	//printf("Debugging: in getino - n = %d\n",n);
  	for(i=0;i<n;i++)
  	{
  		//getchar();
  		printf("local fd is: %d in getino\n", fd); //debugging
  		printf("getino: i=%d name[%d]=%s\n", i, i, path[i]);
  		ino = search(mip,path[i]);
  		printf("Debugging: in getino - ino = %d\n",ino); //debugging
  		if(ino==0)
  		{
  			iput(mip);
  			printf("name %s does not exist\n", path[i]);
       		return 0;
  		}
  		//printf("Debugging: in getino - before iput\n");
  		iput(mip);
  		//printf("Debugging: in getino - before iget\n");
  		mip = iget(fd,ino);
  		//level 3, check for mounted
  		printf("ino of %s is %d\n", pathname, ino); //debugging
		printf("mip -> mounted is: %d\n", mip -> mounted); //debugging
		printf("mip -> dirty is: %d\n", mip -> dirty); //debugging
		printf("mip -> dev is: %d\n", mip -> dev); //debugging

		if(mip -> mounted == 1)
  		{
  			printf("this directory has file system mounted on\n"); //debugging
  			MTABLE *mtable = mip -> mptr;
  			iput(mip);
  			printf("finding mip of mounted filesystem, before ino is %d\n", ino); //debugging
  			mip = getMipOfOpenFile(mtable -> deviceName);
  			printf("ino of mip is now: %d\n", mip -> ino); //debugging
  		}
  		else //debugging
  		{
  			printf("mip is not mounted\n"); //debugging
  		}
  	}
  	//printf("Debugging: in getino - before iput2\n");
  	iput(mip);
  	//printf("Debugging: leaving getino\n");
  	return ino;



}

void mount_root()
{
	//printf("Debugging: in mount_root - before open\n");
	fd = open(dev,O_RDWR);

	if(fd<0)
	{
		printf("Cannot open %s for Read_Write\n",dev);
		exit(0);
	}
	//printf("Debugging: in mount_root - before iget root\n");
	root = iget(dev,2);
	proc[0].cwd = iget(dev,2);
	proc[1].cwd = iget(dev,2);

	get_block(fd,1,buf);
	sp =(SUPER*)buf;

	inode_count = sp->s_inodes_count;
	block_count = sp->s_blocks_count;



	get_block(fd, 2, buf);

	gp = (GD *)buf;


	inode_bitmap = gp->bg_inode_bitmap;
	block_bitmap =  gp->bg_block_bitmap;
	iblock =gp->bg_inode_table;
}
int findCmd(char *command) //helps you parse the cmd list for the matching command name
{
	int i = 0;
	while (cmd[i]) {
		if (strcmp(command, cmd[i]) == 0)
			return i;
		i++;
	}
	return -1;
}

int menu()
{
	printf("Command List :\n");	
	printf("ls\n");
	printf("cd\n");
	printf("pwd\n");
	printf("mkdir\n");
	printf("rmdir\n");
	printf("creat\n");
	printf("link\n");
	printf("unlink\n");
	printf("symlink\n");
	printf("stat\n");
	printf("chmod\n");
	printf("touch\n");
	printf("open\n");
	printf("close\n");
	printf("read\n");
	printf("write\n");
	printf("lseek\n");
	printf("cat\n");
	printf("cp\n");
	printf("mv\n");
	printf("mount\n");
	printf("umount\n");
	printf("menu\n");
	printf("quit\n");

	return 1;
}
void printDirectory(INODE ip)
{
	int blk;
	char*cp;
	char temp[256];
	char buf2[1024];
	char time_s[64];

	//printf("Debugging: in printDirectory - before block assignment\n");
	blk = ip.i_block[0];
	 	get_block(fd,blk,buf2);
	 	dp = (DIR*)buf2;
	 	cp = buf2;
	 	//printf("****************************\n");
 		//printf("inode# rec_len name_len name\n");

	 	while(cp < buf2+BLKSIZE)
	 	{
	 		MINODE *mip;
	 		mip = iget(fd, dp->inode);
	 		INODE *inode1 = &(mip-> INODE);

	 		if(S_ISREG(inode1->i_mode)) printf("r");
			if(S_ISDIR(inode1->i_mode)) printf("d");
			if(S_ISLNK(inode1->i_mode)) printf("l");
			printf((inode1->i_mode & 1 << 8) ? "r" : "-");
			printf((inode1->i_mode & 1 << 7) ? "w" : "-");
			printf((inode1->i_mode & 1 << 6) ? "x" : "-");
			printf((inode1->i_mode & 1 << 5) ? "r" : "-");
			printf((inode1->i_mode & 1 << 4) ? "w" : "-");
			printf((inode1->i_mode & 1 << 3) ? "x" : "-");
			printf((inode1->i_mode & 1 << 2) ? "r" : "-");
			printf((inode1->i_mode & 1 << 1) ? "w" : "-");
			printf((inode1->i_mode & 1 << 0) ? "x" : "-");
	 		strncpy(temp,dp->name,dp->name_len);
	 		temp[dp->name_len] = '\0';
	 		//printf("%d\t%d\t%d\t",dp->inode,dp->rec_len,dp->name_len);
	 		ctime_r((time_t *)&inode1->i_mtime, time_s);
			time_s[strlen(time_s)-1] = 0;
			printf(" %3d%3d %3d%6d %20s ", dp->inode, inode1->i_uid, inode1->i_gid, inode1->i_size, time_s);
	 		if(S_ISLNK(mip->INODE.i_mode))
			{
				printf("%s->%s\n", temp, (char *)mip->INODE.i_block);
				//printf("is link\n"); //debugging
				//printf("mode: %d\n", mip->INODE.i_mode); //debugging
			}
			else
			{
				printf("%s\n", temp);
				//printf("is not link\n"); //debugging
				//printf("mode: %d\n", mip->INODE.i_mode); //debugging
			}
			iput(mip);
	 		cp+=dp->rec_len;
	 		dp = (DIR*)cp;
	 		memset(temp,0,256);
	 	}
}
int ls(char pathname[])
{
	int ino,n=0,i;
	mip = running->cwd;
	char *temp;
	char *backup = malloc(sizeof(char) * strlen(pathname));
	char *pn[64];
	INODE inode = mip->INODE;

	if(mip != root)
	{

		if(inode.i_uid != running -> uid)
		{
			printf("user does not own this directory\n");
			return 0;
		}
		if((inode.i_mode & 1 << 8) == 0)
		{
			printf("user does not have permission to read this directory\n");
			return 0;
		}
	}
	//char*pathNodes[64];
	//getchar();

	strcpy(backup,pathname);

	//printf("Debugging: in ls - before pathname check\n");

	if(strlen(pathname)==0)
	{
		pathname = ".";
	}
	
			ino = getino(pathname);
			mip = iget(fd,ino);
		
		//printf("Debugging: in ls - before printdir function call\n");
		//printf("Debugging: in ls - %d is the inode size\n",mip->INODE.i_size);
		printDirectory(mip->INODE);	
		iput(mip);

}


int cd(char pathname[])
{
	int ino,n,i;
	mip = running->cwd;
	char *temp;
	char *backup = malloc(sizeof(char) * strlen(pathname));
	INODE inode;
	//char temp[256];
	//getchar();

	strcpy(backup,pathname);
	//n=tokenizer(temp,&path,"/");

	/*printf("Debugging: in cd - pathname = %s\n",pathname);
	printf("Debugging: in cd - before pathname parsing\n");
	printf("Debugging: in cd - n value = %d\n",n);
	printf("Debugging: in cd - path[i] value = %s\n",path[i]);*/

	//if(pathname[0])
	//{
		//printf("Debugging: in cd - inside first if statement\n");
		/*if(pathname[0]=='/')
		{
			printf("Debugging: in cd - pathname has /\n");
			mip = iget(fd,2);
		}*/
		//getchar();
		//for(i=0;i<n;i++)
		//{
	if(pathname)
	{
			//getchar();
			//printf("Debugging: in cd - within for loop\n");
			ino = getino(pathname);
			mip = iget(fd,ino);

			//printf("Debugging: in cd - imode = %d\n",mip->INODE.i_mode);
			if(!S_ISDIR(mip->INODE.i_mode))
			{
				printf("Not a directory - cannot cd\n");
				//iput(mip);
				return -1;
			}
			inode = mip->INODE;
			if(inode.i_uid != running -> uid)
			{
				printf("user does not own this directory\n");
				iput(mip);
				return 0;
			}
			if((inode.i_mode & 1 << 8) == 0)
			{
				printf("user does not have permission to read this directory\n");
				iput(mip);
				return 0;
			}
			iput(running->cwd);
			running->cwd = mip;
	}
	else
	{
		printf("Please enter pathname\n");
		return -1;
		//}
	}
	//}
	//else
	//{
		/*printf("Debugging: in cd - starting at root\n");
		mip = iget(fd,2);
		iput(running->cwd);
		running->cwd = mip;*/

	//}
	return 0;
}


int findname(MINODE *parent, int myino, char *name)
{
	int i;
	char  namebuf[256], *cp;
	//printf("Debugging: inside findname\n");
	for(i = 0; i <= 11 ; i ++)
	{
		if(parent->INODE.i_block[i] != 0)
		{
			get_block(parent->dev, parent->INODE.i_block[i], buf);
			dp = (DIR *)buf;
			cp = buf;

			while(cp < &buf[BLKSIZE])
			{
				strncpy(namebuf,dp->name,dp->name_len);
				namebuf[dp->name_len] = 0;

				if(dp->inode == myino)
				{
					strcpy(name, namebuf);
					return 1; 
				}
				cp +=dp->rec_len;
				dp = (DIR *)cp;
			}
		}
	}
	return -1; 
}
int findpino(MINODE*mip,int*myino,int*pino)
{
	char*cp;

	//printf("Debugging: inside findpino\n");
	get_block(fd,mip->INODE.i_block[0],buf);
	dp = (DIR*)buf;
	cp = buf;

	*myino = dp->inode; // '.'
	cp+=dp->rec_len;
	dp=(DIR*)cp;
	*pino=dp->inode; //'..'


}
void rpwd(MINODE*wd)
{
	int myino,parentino;
	char name[256];

	//printf("Debugging: in rpwd\n");

	if(wd-> ino== root->ino)
	{
		return;
	}
	//getchar();
	findpino(wd,&myino,&parentino); // find the inode no.of . and ..
	//printf("Debugging: %d is parentino\n",parentino);
	//printf("Debugging: %d is myino\n",myino);

	wd = iget(fd,parentino);

	//printf("Debugging: after getting parent ino into memory\n");
	findname(wd,myino,name);
	//printf("Debugging: after getting current myino name \n");

	//printf("Debugging: Before recursive rpwd call\n");
	rpwd(wd);

	
	printf("/%s",name);
	iput(wd);


}
int pwd()
{
	if(running->cwd == root)
	{
		printf("/");
	}
	else
	{
		//printf("Debugging: in pwd - Before first call to rpwd\n");
		rpwd(running->cwd);
	}
	printf("\n");

	return 0;
}
int main(int argc , char*argv[])
{

	int flag=1,index=0;
	char s[128];
	char newfile[64];
	if (argc >1)
	{
		dev = argv[1];
	}
	init();
	mount_root();
	while(flag)
	{
		printf("Input a command : ");
		fgets(s, 128, stdin); // input at most 128 chars BUT has \n at end		
		s[strlen(s) - 1] = 0;

		sscanf(s, "%s %s %s", command, pathname, newfile);	
		

		index = findCmd(command);
		switch (index) 
		{
		
			case 1: ls(pathname);       break;
			case 2: cd(pathname);       break;
			case 3: pwd();           	break;
			case 0: menu();				break;
			case 4: flag= 0;			break;
			case 5: mkdir1(pathname);	break;
			case 6: creat1(pathname);	break;
			case 7: rmdir1(pathname);	break;
			case 8: unlink1(pathname);	break;
			case 9: link1(pathname,newfile);	break;
			case 10: symlink1(pathname,newfile); break;
			case 11: readlink1(pathname); break;
			case 12: touch1(pathname); break;
			case 13: chmod1(pathname, newfile); break;
			case 14: stat1(pathname, st); break;
			case 15: close1(atoi(pathname)); break;
			case 16: lseek1(atoi(pathname), atoi(newfile)); break;
			case 17: write1(atoi(pathname)); break;
			case 18: open1(pathname,newfile); break;
			case 19: printOpenFiles(); break;
			case 20: move(pathname, newfile); break;
			case 21: copy(pathname, newfile); break;
			case 22: read1(atoi(pathname), buf, atoi(newfile)); break;
			case 23: cat1(pathname); break;
			case 24: mount1(pathname, newfile); break;
			case 25: umount1(pathname); break;
		}
		memset(s, 0, sizeof(s));

	    memset(command, 0, 64);
	    memset(pathname, 0, 64);

	}


	return 0;
}