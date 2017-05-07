#include "syscall.h"

int
main()
{
	int n;
	for (n=9;n>5;n--) {
		PrintInt(n);
	}
//        Halt();
}
/*

int
main()
{
    int i,j;
    for (i=0;i<50;i++){
        j=0;
        while(j!=10) j++;        
            PrintInt(i);
        }
}
*/
