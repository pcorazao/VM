/*
This software has been developed by Ralph M. Butler for the 
sole use of his students.  No one else may copy any part 
of it for any reason, except with explicit permission.

 Student: Peter Corazao
 Class: OSD Jan 29th 2011
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 

FILE * vDisk;


main(int argc, char *argv[])
{
    int i;

	if ((vDisk = fopen("VDISK","w+b")) == NULL)
    {
		printf("open failed for file: vDisk\n");
		exit(99);
    }
    //just truncated my disk
    
    //go to the end of my file, 
    //and jump 127 integer bytes in i.e. expanding 127 int;
    fseek(vDisk,127,SEEK_END);
    
    
    fclose(vDisk);
}
