V6 FILE SYSTEM 

Objective: To initialise UNIX V6 file system to that supports files up to 4GB. 

Description: 

In fsaccess.c we have doubled all the fields of the super block by increasing then size of the blocks to 1024. Also, we have expanded the free array. We have achived the objective of removing the limitation of 16MB on filesize. 


 To execute program, the following steps are to be followed:
 1. cc fsaccess.c to compile
 2. ./a.out to run the program
 3. initfs command to initialize file system as the following example, where first parameter is the name of the disk, second parameter is the total number of blocks of the disk and the third is the number of i-nodes it should support: 
	
	initfs ./disk 8000 00

After creating file system commands from 4 to 7 are available for executing: 

4. Create a new file called v6-file in the v6 file system and fill the contents of the newly created file 
with the contenrs of the external file as follows:
	
	cpin externalfile v6-file
	
5. If the v6-file exists, create externalfile and make the externalfile's contents equal to v6-file:
	
	cpout v6-file externalfile
	
6. If v6-dir doesn't exist, create the directory and set its first two entries . and .. :
	
	mkdir v6-dir
	
7. remove the file if exists:
 	
	rm v6-file

8. to exit enter q


Project Done By:
Balnur Duisenbayeva, Gaurav Sawant
