#include <stdio.h>
#define SZ 10000000

int main(){
	FILE *fpw = fopen("fin.txt", "w+");
	for(int i = 0; i < SZ/5; i++)	
    	fprintf(fpw, "abcde");
    fclose(fpw);
}
