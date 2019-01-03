/* Unix File system Modified*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>


#define BLOCK_SIZE 1024
#define ISIZE 64
#define inode_alloc 0100000        //Flag value to allocate an inode
#define pfile 000000               //To define file as a plain file
#define lfile 010000               //To define file as a large file
#define directory 040000           //To define file as a Directory
#define array_max_chain 256
//#define null_size 1024


//super block structure
typedef struct {
    unsigned short isize;
    unsigned short fsize;
    unsigned short nfree;
    unsigned short free[100];
    unsigned short ninode;
    unsigned short inode[100];
    char flock;
    char ilock;
    char fmod;
    unsigned short time[2];
} superb;

// directory contents
typedef struct {
    unsigned short inode;
    char filename[13];
} dir;

//inode structure
typedef struct {
    unsigned short flags;
    char nlinks;
    char uid;
    char gid;
    char size0;
    unsigned int size1;
    unsigned short addr[24];
    unsigned short actime[1];
    unsigned short modtime[2];
} filesys_inode;

filesys_inode initial_inode;
superb super;


int fd ;		//file descriptor
int rootfd;
const char *rootname;
unsigned short arry_chain[array_max_chain];
dir newdir;
dir dir1;

// functions used
unsigned short get_free_data_block();
int fs_init(char* path, unsigned short parse_blocks,unsigned short total_inodes);
void root_dir();
void blk_read(unsigned short *target, unsigned short block_entry_num);
void block_to_array(unsigned short *target, unsigned short block_entry_num);
void copy_into_inode(filesys_inode current_inode, unsigned int new_inode);
void freeblock(unsigned short block);
void blocks_chains(unsigned short parse_blocks);
unsigned short allocateinode();
void cpin(const char *pathname1 , const char *pathname2);
void out_plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void out_largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void cpout(const char *pathname1 , const char *pathname2);
void largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated);
void makedir(const char *pathname);
void update_rootdir(const char *pathname , unsigned short in_number);
void remove_file(const char *pathname);

//file system initilization function
int fs_init(char* path, unsigned short parse_blocks,unsigned short total_inodes) {
    printf("\n filesystem initialization started \n");
    char buffer[BLOCK_SIZE];
    int no_of_bytes;
    if(((total_inodes*64)%1024) == 0)
        super.isize = ((total_inodes*64)/1024);
    else
        super.isize = ((total_inodes*64)/1024)+1;

    super.fsize = parse_blocks;

    unsigned short i = 0;

    if((fd = open(path,O_RDWR|O_CREAT,0600))== -1) {
        printf("\n file opening error [%s]\n",strerror(errno));
        return 1;
    }

    for (i = 0; i<100; i++)
        super.free[i] =  0;

    super.nfree = 0;
    super.ninode = 100;
    for (i=0; i < 100; i++)
        super.inode[i] = i;

    super.flock = 'f';
    super.ilock = 'i';
    super.fmod = 'f';
    super.time[0] = 0000;
    super.time[1] = 1970;
    lseek(fd,BLOCK_SIZE,SEEK_SET);

    lseek(fd,0,SEEK_SET);
    write(fd, &super, 1024);

     if((no_of_bytes =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE){
         printf("\nERROR : error in writing the super block\n");
         return 0;
     }

    for (i=0; i<BLOCK_SIZE; i++)
        buffer[i] = 0;
    lseek(fd,1*1024,SEEK_SET);
    for (i=0; i < super.isize; i++)
        write(fd,buffer,BLOCK_SIZE);

    // calling chaining data blocks procedure
    blocks_chains(parse_blocks);

    for (i=0; i<100; i++){
        super.free[super.nfree] = i+2+super.isize; //get a free block
        ++super.nfree;
    }
    root_dir();
    return 1;
}

//function to read integer array from the required block
void blk_read(unsigned short *target, unsigned short block_entry_num) {
    int flag=0;
    if (block_entry_num > super.isize + super.fsize )
        flag = 1;
	else {
        lseek(fd,block_entry_num*BLOCK_SIZE,SEEK_SET);
        read(fd, target, BLOCK_SIZE);
	}
}

//function to write integer array to the required block, writing array to block
void block_to_array(unsigned short *target, unsigned short block_entry_num) {
    int flag1, flag2;
	int no_of_bytes;

    if (block_entry_num > super.isize + super.fsize )
		flag1 = 1;
	else{
	    lseek(fd,block_entry_num*BLOCK_SIZE,0);
        no_of_bytes=write(fd, target, BLOCK_SIZE);

	    if((no_of_bytes) < BLOCK_SIZE)
		    flag2=1;
	}
    if (flag2 == 1) {
        printf("problem with block number %d",block_entry_num);
    }
}

// Data blocks chaining procedure
void blocks_chains(unsigned short parse_blocks) {
    unsigned short emptybuffer[256];
    unsigned short linked_count;
    unsigned short partitions_blks = parse_blocks/100;
    unsigned short blks_not_in_partitions = parse_blocks%100;
    unsigned short index = 0;
    int i=0;
    for (index=0; index <= 255; index++){
    	emptybuffer[index] = 0;
        arry_chain[index] = 0;
    }

    //chaining for chunks of blocks 100 blocks at a time
    for (linked_count=0; linked_count < partitions_blks; linked_count++){
        arry_chain[0] = 100;

        for (i=0;i<100;i++) {
            if((linked_count == (partitions_blks - 1)) && (blks_not_in_partitions == 0)) {
                if ((blks_not_in_partitions == 0) && (i==0)){
                    if ((linked_count == (partitions_blks - 1)) && (i==0)){
                        arry_chain[i+1] = 0;
                        //continue;
                    }
                }
            }
            arry_chain[i+1] = i+(100*(linked_count+1))+(super.isize + 2 );

        }
        block_to_array(arry_chain, 2+super.isize+100*linked_count);

        for (i=1; i<=100;i++)
            block_to_array(emptybuffer, 2+super.isize+i+ 100*linked_count);
    }

    //chaining for remaining blocks
    arry_chain[0] = blks_not_in_partitions;
    arry_chain[1] = 0;
    for (i=1;i<=blks_not_in_partitions;i++)
        arry_chain[i+1] = 2+super.isize+i+(100*linked_count);

    block_to_array(arry_chain, 2+super.isize+(100*linked_count));

    for (i=1; i<=blks_not_in_partitions;i++)
        block_to_array(arry_chain, 2+super.isize+1+i+(100*linked_count));
}


//function to write to an inode given the inode number
void copy_into_inode(filesys_inode current_inode, unsigned int new_inode){
    int no_of_bytes;
    lseek(fd,2*BLOCK_SIZE + new_inode*ISIZE,0);
    no_of_bytes=write(fd,&current_inode,ISIZE);

    if((no_of_bytes) < ISIZE)
	    printf("\n Error in inode number : %d\n", new_inode);
}

//function to create root directory and its corresponding inode.
void root_dir() {
    rootname = "root";
    rootfd = creat(rootname, 0600);
    rootfd = open(rootname , O_RDWR | O_APPEND);
    unsigned int i = 0;
    unsigned short no_of_bytes;
    unsigned short datablock = get_free_data_block();
    for (i=0;i<14;i++)
        newdir.filename[i] = 0;

    newdir.filename[0] = '.';			//root directory's file name is .
    newdir.filename[1] = '\0';
    newdir.inode = 1;					// root directory's inode number is 1.

    initial_inode.flags = inode_alloc | directory | 000077;   		// flag for root directory
    initial_inode.nlinks = 2;
    initial_inode.uid = '0';
    initial_inode.gid = '0';
    initial_inode.size0 = '0';
    initial_inode.size1 = ISIZE;
    initial_inode.addr[0] = datablock;
    for (i=1;i<24;i++)
        initial_inode.addr[i] = 0;

    initial_inode.actime[0] = 0;
    initial_inode.modtime[0] = 0;
    initial_inode.modtime[1] = 0;

    copy_into_inode(initial_inode, 0);
    lseek(rootfd , 1024 , SEEK_SET);
    write(rootfd , &initial_inode , 64);
    lseek(rootfd, datablock*BLOCK_SIZE, SEEK_SET);

    //filling 1st entry with .
    no_of_bytes = write(rootfd, &newdir, 16);
    //dir.inode = 0;
    if((no_of_bytes) < 16)
        printf("\n Error in writing root directory \n ");

    newdir.filename[0] = '.';
    newdir.filename[1] = '.';
    newdir.filename[2] = '\0';
    // filling with .. in next entry(16 bytes) in data block.

    no_of_bytes = write(rootfd, &newdir, 16);
    if((no_of_bytes) < 16)
        printf("\n Error in writing root directory\n ");
    close(rootfd);
}

//free data blocks and initialize free array
void freeblock(unsigned short block){           // not used yet, should be used while removing file
    super.free[super.nfree] = block;
    ++super.nfree;
}

//function to get a free data block. Also decrements nfree for each pass
unsigned short get_free_data_block() {
    unsigned short block;
    super.nfree--;
    block = super.free[super.nfree];
    super.free[super.nfree] = 0;

    if (super.nfree == 0){
        int n=0;
        blk_read(arry_chain, block);
        super.nfree = arry_chain[0];
        for(n=0; n<100; n++)
            super.free[n] = arry_chain[n+1];
    }
    return block;
}

//Function to allocate inode
unsigned short allocateinode(){
    unsigned short inumber;
    unsigned int i = 0;
    super.ninode--;
    inumber = super.inode[super.ninode];
    return inumber;
}

//Main function
int main(int argc, char *argv[]){
    char c;
    system("clear");
    int i=0;
    char *tmp = (char *)malloc(sizeof(char) * 200);
    char *cmd1 = (char *)malloc(sizeof(char) * 200);
    signal(SIGINT, SIG_IGN);
    int filesys_count = 0;
    char *my_argv, cmd[256];
    unsigned short n = 0, j , k;
    char buffer1[BLOCK_SIZE];
    unsigned short no_of_bytes;
    char *name_direct,*cpoutext, *cpoutsource, *cpinext, *cpinsource, *fs_path, *rmfile;
    unsigned int blk_no =0, inode_no=0;
    char *arg1, *arg2, *num3, *num4;

    printf("\n\nUnix_File_system_V6 ");
    fflush(stdout);
    printf("\n>>");

    while(1){
        scanf(" %[^\n]s", cmd);
        my_argv = strtok(cmd," ");

        if(strcmp(my_argv, "initfs")==0){

            fs_path = strtok(NULL, " ");
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");

            if(access(fs_path, X_OK) != -1){
                printf("filesystem already exists. \n");
                printf("same file system will be used\n");
                filesys_count=1;
            }
            else{
                if (!arg1 || !arg2)
                    printf(" insufficient arguments to proceed\n");
                else{
                    blk_no = atoi(arg1);
                    inode_no = atoi(arg2);
                    if(fs_init(fs_path,blk_no, inode_no)){
                        printf("file system initialized \n");
                        filesys_count = 1;
                    }
                    else {
                        printf("Error:file system cannot be initialized \n");
                    }
                }
            }
            my_argv = NULL;
            printf("\n>>");
        }
	    else if(strcmp(my_argv, "mkdir")==0){
	        if(filesys_count == 0)
	            printf("cant see init_FS. Retry after initializing file system\n");
	        else{
	            name_direct = strtok(NULL, " ");
	            if(!name_direct){
		            printf("No directory name entered. Please retry\n");
		        }
		        else{
	                unsigned int dirinum = allocateinode();
	                if(dirinum < 0){
		                printf("insufficient inodes \n");
		                exit(1);
		            }
                    if(access(name_direct, R_OK) == 0){
                        printf("Directory already exists\n");
                    }
                    else
                        makedir(name_direct);
	            }
	        }
	        my_argv = NULL;
            printf("\n>>");
        }
	    else if(strcmp(my_argv, "cpin")==0){
	        if(filesys_count == 0)
	            printf("cant see init_FS. Retry after initializing file system\n");
	        else{
	            cpinsource = strtok(NULL, " ");
	            cpinext = strtok(NULL, " ");
	            if(!cpinsource || !cpinext )
	                printf("Insufficient paths \n");
	            else if((cpinsource) && (cpinext )){
	                cpin(cpinsource,cpinext);
	            }
	        }
	        my_argv = NULL;
            printf("\n>>");
	    }
	    else if(strcmp(my_argv, "cpout")==0){
	        if(filesys_count == 0)
	            printf("cant see init_FS. Retry after initializing file system\n");
	        else{
	            cpoutsource = strtok(NULL, " ");
	            cpoutext = strtok(NULL, " ");

	            if(!cpoutsource || !cpoutsource )
	                printf("Required file names(source and target file names) have not been entered. Please retry\n");
	            else if((cpinsource) && (cpinext )){
	                cpout(cpoutsource,cpoutext);
	            }
	        }
	        my_argv = NULL;
            printf("\n>>");
	    }
		else if(strcmp(my_argv, "rm")==0){
    	    if(filesys_count == 0)
    	    printf("cant see init_FS. Retry after initializing file system\n");
    	    else{
    	        rmfile = strtok(NULL, " ");
    	        if(!rmfile){
    		        printf("No file name entered. Please retry\n");
    		    }
    		    else{
    	            remove_file(rmfile);
    	        }
    	    }
    	    my_argv = NULL;
            printf("\n>>");
        }
	    else if(strcmp(my_argv, "q")==0){
	        printf("\n quitting file system...");
	        printf("\nThank you\n");
	        lseek(fd,BLOCK_SIZE,0);

	        if((no_of_bytes =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE){
	            printf("\nerror in writing the super block");
	        }
		    lseek(fd,BLOCK_SIZE,0);
	        return 0;
	    }
	    else{
            printf("\nInvalid command\n ");
            printf("\n>>");
	    }
	}
}

//Function to copy file contents from external file to mv6 filesystem
void cpin(const char *pathname1 , const char *pathname2){
    struct stat stats;
    int blksize , blks_allocated , req_blocks;
    int filesize;
    stat(pathname1 , &stats);
    blksize = stats.st_blksize;
    blks_allocated = stats.st_blocks;
    filesize = stats.st_size;
    req_blocks = filesize / 1024;
    if(blks_allocated <= 24){
        printf("plain file , %d\n" , blks_allocated);
        plainfile(pathname1 , pathname2 , blks_allocated); // If the external is a small file
    }
    else{
   	    printf("Large file , %d\n" , blks_allocated);
   	    largefile(pathname1 , pathname2 , blks_allocated); // If external file is a large file
    }
}

//Function to copy from a Small File into mv6 File system
void plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated){
    int fdes , fd1 ,i ,j,k,l;
    unsigned short size;
    unsigned short i_number;
    i_number = allocateinode();
    struct stat s1;
    stat(pathname1 , &s1);
    size = s1.st_size;
    filesys_inode in;
    in.flags = inode_alloc | pfile | 000077;
    in.size0 =0;
    in.size1 = size;
    int blks_allocated = size/1024;
    printf("alloc  , %d\n" , blks_allocated);
    fdes = creat(pathname2, 0775);
    fdes = open(pathname2 , O_RDWR | O_APPEND);
    lseek(fdes , 0 , SEEK_SET);
    update_rootdir(pathname2 , i_number);
    for(i=0; i<blks_allocated; i++){
        in.addr[i] = get_free_data_block();
    }
    write(fdes , &in , 64);
    close(fdes);
    unsigned int bf[256];
    fd1 = open(pathname1 ,O_RDONLY);
    fdes = open(pathname2 , O_RDWR | O_APPEND);
    for(j =0; j<=blks_allocated; j++){
        lseek(fd1 , 1024*j , SEEK_SET);
        read(fd1 ,&bf , 1024);
        lseek(fdes , 1024*(in.addr[j]) , SEEK_SET);
        write(fdes , &bf , 1024);
    }
    close(fdes);
    close(fd1);
    fd = open(pathname2 , O_RDONLY);
}

//Function to copy from a Large File into mv6 File system
void largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated){
    int size ,fdes1 ,fd1 ,i ,j ,h ,z ,x ,v , icount , c ,d ,m, w ,e ,t , l,f,sizer;
    unsigned short buff[100];
    unsigned short b;
    unsigned short fb;
    unsigned short i_number;
    i_number = allocateinode();
    struct stat s1;
    stat(pathname1 , &s1);
    size = s1.st_size;
    int blks_allocated = size/1024;
    filesys_inode in;
    in.flags = inode_alloc | lfile | 000077;
    fdes1 = creat(pathname2, 0775);
    fdes1 = open(pathname2 , O_RDWR | O_APPEND);
    lseek(fdes1 , 0 , SEEK_SET);
    for(i=0; i<24; i++){
        in.addr[i] = get_free_data_block();
    }
    write(fdes1 , &in , 64);
    close(fdes1);
    if(blks_allocated > 67336){
        printf("\nExternal File too large\n");
    }
    else{
        if(blks_allocated < 2048){
            unsigned int bf[24][256];
            int k =24;
            for(j =0; j<24; j++){
                if(k < blks_allocated){
                    fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                    for(m=0; m<256; m++){
                        if(m > blks_allocated)
                            break;
                        bf[j][m] = get_free_data_block();
                    }
                    close(fdes1);
                    for(e=0; e<256; e++){
                        fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                        lseek(fdes1 , ((in.addr[j])*1024)+(2*e) , SEEK_SET);
                        write(fdes1 , &bf[j][e] , 2);
                        close(fdes1);
                    }
                }
                else
                  break;
                k = k + 256;
            }

            unsigned int buffer[256];
            unsigned int bu[256][256];
            int m=0;
            int r=0;
            int count =0;
            fd1 = open(pathname1 , O_RDONLY);
            for(h=0; h<=blks_allocated; h++){
                if(r<24){
                    lseek(fd1 , h*1024 , SEEK_SET);
                    for(x=0; x<256; x++){
                        if(count > blks_allocated)
                            break;
                        read(fd1 , &buffer , 1024);
                        fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                        lseek(fdes1 , 1024*(bf[r][x]) , SEEK_SET);
                        write(fdes1 , &buffer , 1024);
                        close(fdes1);
                        count++;
                    }
                }
                r++;
            }
            close(fd1);
        }
        else{
            unsigned int bf[24][256];
            int k =23;
            int r=0;
            fdes1 = open(pathname2 , O_RDWR | O_APPEND);
            for(j =0; j<23; j++){
                if(k < blks_allocated){
                    fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                    for(m=0; m<256; m++){
                        bf[j][m] = get_free_data_block();;
                        r++;
                    }
                    close(fdes1);
                    fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                    for(e=0; e<256; e++){
                        lseek(fdes1 , ((in.addr[j])*1024)+ 2*e , SEEK_SET);
                        write(fdes1 , &bf[j][e] , 2);
                    }
                    close(fdes1);
                }
                else
                    break;
                k = k + 256;
            }
            //code for double indirect block
            unsigned int lbuff[256][256];
            fdes1 = open(pathname2 , O_RDWR | O_APPEND);
            for(c=0; c<256; c++){
                lseek(fdes1 , bf[23][c]*1024 , SEEK_SET);
                for(d=0; d<256;d++){
                    if(r > blks_allocated)
                        break;
                    lbuff[c][d] = get_free_data_block();;
                    r++;
                }
                for(t=0; t<256; t++){
                    lseek(fdes1 , (bf[23][c]*1024)+2*t , SEEK_SET);
                    write(fdes1 , &lbuff[c][t] , 2);
                }
            }
            close(fdes1);
            unsigned int buffer[256];
            int s=0;
            int ko=0;
            fd1 = open(pathname1 , O_RDONLY);
            for(h=0; h<=blks_allocated; h++){
                if(ko<23){
                    lseek(fd1 , h*1024 , SEEK_SET);
                    read(fd1 , &buffer , 1024);
                    for(x=0; x<256; x++){
                        fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                        lseek(fdes1 , (bf[ko][x])*1024 , SEEK_SET);
                        write(fdes1 , &buffer , 1024);
                        close(fdes1);
                        s++;
                    }
                    ko++;
                }
                else{
                    lseek(fd1 , h*1024 , SEEK_SET);
                    read(fd1 , &buffer , 1024);
                    unsigned int dilbuf[256];
                    fdes1 = open(pathname2 , O_RDWR | O_APPEND);
                    for(v=0; v<256; v++){
                        for(w=0; w<256; w++){
                            if(s > blks_allocated)
                                break;
                            lseek(fdes1 , lbuff[v][w]*1024 , SEEK_SET);
                            write(fdes1 , &buffer , 1024);
                            s++;
                        }
                    }
                    close(fdes1);
                }
            }
            close(fd1);
        }
    }
}

//Function to copy from mv6 File System into an external File
void cpout(const char *pathname1 , const char *pathname2){
    struct stat stats;
    int blksize , blks_allocated , no_of_blocks;
    int filesize;
    stat(pathname1 , &stats);
    blksize = stats.st_blksize;
    blks_allocated = stats.st_blocks;
    filesize = stats.st_size;
    no_of_blocks = filesize /1024;
    if(blks_allocated < 24){
   	    printf("plainfile , %d\n" , no_of_blocks);
        out_plainfile(pathname1 , pathname2 , no_of_blocks);  //If MV6 File is a small file
    }
    else{
        printf("largefile , %d\n" , no_of_blocks);
        out_largefile(pathname1 , pathname2 ,  no_of_blocks);  //If MV6 File is a large file
    }
}

//Function to copy Small file from mv6 File System into an external File
void out_plainfile(const char *pathname1 , const char *pathname2 , int blocks_allocated){
    int nblocks = blocks_allocated;
    int fdes , fd1 , i;
    unsigned int buffer[256];
    fdes = open(pathname1 , O_RDONLY);
    fd1 = creat(pathname2, 0775);
    fd1 = open(pathname2, O_RDWR | O_APPEND);
    for(i =0 ; i<nblocks; i++){
        lseek(fdes , i*1024 , SEEK_SET);
        read(fdes , &buffer , 1024);
        lseek(fd1 , 1024*i , SEEK_SET);
        write(fd1 , &buffer , 1024);
    }
    close(fd1);
    close(fdes);
}

// Function to copy a large file from mv6 File System into an external File
void out_largefile(const char *pathname1 , const char *pathname2 , int blocks_allocated){
    int nblocks = blocks_allocated;
    int fdes1 , fd1 , i;
    unsigned int buffer[256];
    fd1 = creat(pathname2, 0775);
    fd1 = open(pathname2 , O_RDWR | O_APPEND);
    fdes1 = open(pathname1 , O_RDONLY);
    for(i=0; i<nblocks; i++){
        lseek(fdes1 , i*1024 , SEEK_SET);
        read(fdes1 , &buffer , 1024);
        lseek(fd1 , i*1024 , SEEK_SET);
        write(fd1 , &buffer , 1024);
    }
    close(fd1);
    close(fdes1);
}

// Function to update root directory
void update_rootdir(const char *pathname , unsigned short in_number){
    int i;
    dir ndir;
    int size;
    ndir.inode = in_number;
    strncpy(ndir.filename, pathname , 14);
    size = write(rootfd , &ndir , 16);
}
// Function to create a Directory
void makedir(const char *pathname){
    int filedes , i ,j;
    filedes = creat(pathname, 0666);
    filedes = open(pathname , O_RDWR | O_APPEND);
    filesys_inode ino1;
    unsigned short inum;
    inum = allocateinode();
    ino1.flags = inode_alloc | directory | 000077;
    ino1.nlinks = 2;
    ino1.uid =0;
    ino1.gid =0;
    for(i=0; i<24; i++){
        ino1.addr[i] = 0;
    }
    unsigned short bk;
    bk = get_free_data_block();
    ino1.addr[0] = bk;
    lseek(filedes , 1024 , SEEK_SET);
    write(filedes , &ino1 , 64);
    dir direct;
    direct.inode = inum;
    strncpy(direct.filename, "." , 14);
    lseek(filedes , ino1.addr[0]*1024 , SEEK_SET);
    write(filedes , &direct , 16);
    strncpy(direct.filename, ".." , 14);
    lseek(filedes , (ino1.addr[0]*1024 + 16) , SEEK_SET);
    write(filedes , &direct , 16);
    close(filedes);
    update_rootdir(pathname , inum);
    printf("directory created\n");
}

// Function to remove file from mv6 filesystem
void remove_file(const char *pathname){
    int fd;
    fd = open(pathname , O_RDWR);
    if(fd== -1){
        printf("File doesn't exist");
    }
    else{
        close(fd);
        unsigned short size;
        unsigned short i_number;
        int blksize , blks_allocated , no_of_blocks;
        int filesize;
        struct stat stats;
        stat(pathname , &stats);
        size = s1.st_size;
        i_number = s1.st_ino;
        int blks_allocated = size/1024;
        printf("alloc  , %d\n" , blks_allocated);
        printf("inode  , %d\n" , super.inode[i_number]);
        blks_allocated = stats.st_blocks;

        remove(pathname);
    }
}
