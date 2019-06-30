//
// Simple FIle System
// Student Name : 조용성
// Student Number : B411203
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

void sfs_touch(const char* path)
{
	//skeleton implementation
	int i,j,k;
	struct sfs_inode si;
	int bitmap[SFS_BITMAPSIZE(spb.sp_nblocks)];
	
	struct sfs_inode check;
	
	for(k=0;k<spb.sp_nblocks;k++)
	{
		disk_read( &check, k);
		if(check.sfi_size > 0)
			BIT_SET(bitmap[k],0);
		else
			BIT_CLEAR(bitmap[k],0);
	}
	disk_read( &si, sd_cwd.sfd_ino );
	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );
	
	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	int newbie_ino;
	for(k=2+SFS_BITBLOCKS(spb.sp_nblocks);k<SFS_BITMAPSIZE(spb.sp_nblocks);k++)
	{
		if(BIT_CHECK(bitmap[k],0) == 0)
		{
			newbie_ino = k;
			break;
		}
		else if(BIT_CHECK(bitmap[k],0) != 0)
			continue;
		else
			error_message("ERROR",path,-7);
	}

	//block access
	bool exit = false;
	for(i=0; i<SFS_NDIRECT; i++)
	{
		if( si.sfi_direct[i] != 0)
		{
			 disk_read( sd, si.sfi_direct[i] );
			for(j=0;j<SFS_DENTRYPERBLOCK;j++)
			{
				if(sd[j].sfd_ino == 0)
				{
					sd[j].sfd_ino = newbie_ino; // empty block
					exit = true;
					break;
				}
			}
			if(exit = true)
				break;
				
		}
		else
		{
			disk_read( sd, si.sfi_direct[i]);
			j=2;
			sd[j].sfd_ino = newbie_ino;
			break;
		}
	}
	strncpy ( sd[j].sfd_name, path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[i] );
	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write( &newbie, newbie_ino );
}

void sfs_cd(const char* path)
{
	int i,j;
	struct sfs_inode inode;
	struct sfs_inode cnode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	disk_read( &inode, sd_cwd.sfd_ino);
	for(j=0;j<SFS_NDIRECT;j++)
	{
	disk_read( dir, inode.sfi_direct[j]);
	if(inode.sfi_direct[j] == 0) break;

	for(i=0;i<SFS_DENTRYPERBLOCK;i++)
	{
		disk_read( &cnode, dir[i].sfd_ino);
		if(cnode.sfi_type == SFS_TYPE_FILE && !strcmp(dir[i].sfd_name,path) )
		{
			error_message("ERROR",path,-2);
			break;
		}
		else if( cnode.sfi_type == SFS_TYPE_DIR && !strcmp(dir[i].sfd_name,path) )
		{
			sd_cwd = dir[i];
			break;
		}
		else if(j == (SFS_NDIRECT-1))
		{
			error_message("ERROR",path,-1);
			break;
		}
	}
	}
}

void sfs_ls(const char* path)
{
	int i,j,k;
	struct sfs_inode inode;	//	i-node
	struct sfs_inode cnode;	//	child i-node
	disk_read( &inode, sd_cwd.sfd_ino);	//cwd ino -> inode
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	for(j=0;j<SFS_NDIRECT;j++)
	{
	if(inode.sfi_direct[j] == 0) continue;	//inode 블록체크 0일때 break
		if(inode.sfi_type == SFS_TYPE_DIR)		//cnode type == dir
		{

			disk_read( dir, inode.sfi_direct[j]);
			for(i=0;i< SFS_DENTRYPERBLOCK; i++)
			{
					disk_read( &cnode, dir[i].sfd_ino);
					if(cnode.sfi_type == SFS_TYPE_DIR )
					{
					printf("%s",dir[i].sfd_name);
					printf("/");
					printf("\t");
					}
					else if(cnode.sfi_type == SFS_TYPE_FILE)
					{	
					printf("%s",dir[i].sfd_name);
					printf("\t");
					}
					else
					continue;	
			}		
		}
		else if(inode.sfi_type == SFS_TYPE_FILE)
			error_message("ERROR",path,-5);

		else
		break;
	}
	printf("\n");
}

void sfs_mkdir(const char* org_path)
{ 
	
 	int i,j,k;
         struct sfs_inode si;
         int bitmap[SFS_BITMAPSIZE(spb.sp_nblocks)];

         struct sfs_inode check;

         for(k=0;k<spb.sp_nblocks;k++)
         {
                 disk_read( &check, k);
                 if(check.sfi_size > 0)
                         BIT_SET(bitmap[k],0);
                 else
                         BIT_CLEAR(bitmap[k],0);
         }
         disk_read( &si, sd_cwd.sfd_ino );
         //for consistency
                  assert( si.sfi_type == SFS_TYPE_DIR );
       	  struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	struct sfs_inode snode;

         int newbie_ino;
         for(k=2+SFS_BITBLOCKS(spb.sp_nblocks);k<SFS_BITMAPSIZE(spb.sp_nblocks);k++)
         {
                 if(BIT_CHECK(bitmap[k],0) == 0)
                 {
                         newbie_ino = k;
                         break;
                 }
                 else if(BIT_CHECK(bitmap[k],0) != 0)
                         continue;
                 else
		{
                         error_message("ERROR",org_path,-7);
		}
         }

         bool exit = false;
         for(i=0; i<SFS_NDIRECT; i++)
         {
                 if( si.sfi_direct[i] != 0)
                 {
                          disk_read( sd, si.sfi_direct[i] );
                         for(j=0;j<SFS_DENTRYPERBLOCK;j++)
                         {
                                 if(sd[j].sfd_ino == 0)
                                 {
                                         sd[j].sfd_ino = newbie_ino; // empty block
                                         exit = true;
                                         break;
                                 }
                         }
                         if(exit = true)
                                 break;

                 }
                 else
                 {
                         disk_read( sd, si.sfi_direct[i]);
                         j=2;
                         sd[j].sfd_ino = newbie_ino;
                         break;
                 }
         }
	   strncpy ( sd[j].sfd_name, org_path, SFS_NAMELEN );

         disk_write( sd, si.sfi_direct[i] );
         si.sfi_size += sizeof(struct sfs_dir);
         disk_write( &si, sd_cwd.sfd_ino );

         struct sfs_inode newbie;
	 struct sfs_dir newdir[SFS_DENTRYPERBLOCK];

         bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
         newbie.sfi_size = sizeof(struct sfs_dir);
         newbie.sfi_type = SFS_TYPE_DIR;
	 disk_read( newdir, newbie.sfi_direct[0]); 
	 newdir[0].sfd_ino = newbie_ino;
         newdir[1].sfd_ino = si.sfi_direct[i];


         disk_write( &newbie, newbie_ino );
		
}

void sfs_rmdir(const char* org_path) 
{
	    int i,j,k;
          struct sfs_inode si;
          int bitmap[SFS_BITMAPSIZE(spb.sp_nblocks)];

          struct sfs_inode check;

          for(k=0;k<spb.sp_nblocks;k++)
          {
                  disk_read( &check, k);
                 if(check.sfi_size > 0)
                          BIT_SET(bitmap[k],0);
                  else
                          BIT_CLEAR(bitmap[k],0);
          }
          disk_read( &si, sd_cwd.sfd_ino );
          //for consistency
                   assert( si.sfi_type == SFS_TYPE_DIR );
           struct sfs_dir sd[SFS_DENTRYPERBLOCK];
         struct sfs_inode snode;

          bool exit = false;
	        for(i=0; i<SFS_NDIRECT; i++)
          {
                  if( si.sfi_direct[i] != 0)
                  {
                          disk_read( sd, si.sfi_direct[i] );
                          for(j=0;j<SFS_DENTRYPERBLOCK;j++)
                          {
                                  if(!strcmp(sd[j].sfd_name,org_path))
                                  {
                                          sd[j].sfd_ino = 0; // 지워야할 inode번호 초기화
                                          exit = true;
                                          break;
                                  }
                          }
                          if(exit = true)
                                  break;

                  }
          }
          disk_write( sd, si.sfi_direct[i] );
          si.sfi_size -= sizeof(struct sfs_dir);
          disk_write( &si, sd_cwd.sfd_ino );
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	int i,j;
	bool exit=false;
	struct sfs_inode snode;
	struct sfs_inode fnode;
	disk_read(&snode, sd_cwd.sfd_ino);
	struct sfs_dir check[SFS_DENTRYPERBLOCK];
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	for(j=0;j<SFS_NDIRECT;j++)
	{
	disk_read( check, snode.sfi_direct[j]);

	for(i=2;i<SFS_DENTRYPERBLOCK;i++)
	{
		if(!strcmp(check[i].sfd_name,dst_name))
		{
			error_message("ERROR",dst_name,-6);
			exit = true;
			break;
		}
	}
	if(exit = true)
		break;
	}

	for(j=0;j<SFS_NDIRECT;j++)
	{
		disk_read( dir, snode.sfi_direct[j]);
		for(i=2;i<SFS_DENTRYPERBLOCK;i++)
		{
		 if(!strcmp(dir[i].sfd_name,src_name))
		{
			strncpy ( dir[i].sfd_name,dst_name,SFS_NAMELEN);
			disk_write( dir,snode.sfi_direct[j]);
			disk_write( &snode, sd_cwd.sfd_ino);
			exit = true;
			break;
		}
		else if( i = (SFS_DENTRYPERBLOCK-1))
		continue;
	 	else
		{
		error_message("ERROR",src_name,-1);	
		}
		}
	if(exit = true)
		break;
	}
//	printf("Not Implemented\n");
}

void sfs_rm(const char* path) 
{
	            int i,j,k;
           struct sfs_inode si;
           int bitmap[SFS_BITMAPSIZE(spb.sp_nblocks)];

           struct sfs_inode check;

           for(k=0;k<spb.sp_nblocks;k++)
           {
                   disk_read( &check, k);
                  if(check.sfi_size > 0)
                           BIT_SET(bitmap[k],0);
                   else
                           BIT_CLEAR(bitmap[k],0);
           }
           disk_read( &si, sd_cwd.sfd_ino );
           //for consistency
                    assert( si.sfi_type == SFS_TYPE_DIR );
            struct sfs_dir sd[SFS_DENTRYPERBLOCK];
          struct sfs_inode snode;
	  struct sfs_inode cnode;
           bool exit = false;
                 for(i=0; i<SFS_NDIRECT; i++)
           {
                   if( si.sfi_direct[i] != 0)
                   {
                           disk_read( sd, si.sfi_direct[i] );
                           for(j=0;j<SFS_DENTRYPERBLOCK;j++)
                           {
                                   if(!strcmp(sd[j].sfd_name,path))
                                   {
					   disk_read( &snode, sd[j].sfd_ino);
					   for(k=0;k<SFS_NDIRECT;k++)	
						{
							disk_read(&cnode,sd[k].sfd_ino);
							BIT_CLEAR(sd[k].sfd_ino,0);
							bzero(&cnode,SFS_BLOCKSIZE);
							snode.sfi_size -= sizeof(struct sfs_dir);
						}
					   disk_read( &cnode,snode.sfi_indirect);
						bzero(&cnode,SFS_BLOCKSIZE);
						
                                           sd[j].sfd_ino = 0; // 지워야할 inode번호 초기화
					   BIT_CLEAR(sd[j].sfd_ino,0);
					   bzero(&snode,SFS_BLOCKSIZE);
                                           exit = true;
                                          break;
                                   }
                           }
                           if(exit = true)
                                   break;

                   }
           }
           disk_write( sd, si.sfi_direct[i] );
           si.sfi_size -= sizeof(struct sfs_dir);
           disk_write( &si, sd_cwd.sfd_ino );

}

void sfs_cpin(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");
}
