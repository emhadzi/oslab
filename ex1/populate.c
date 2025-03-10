#include <stdio.h>
#define SZ 1000000

int main(){
	FILE *fpw = fopen("fin.txt", "w+");
	for(int i = 0; i < SZ/6; i++)	
    	fprintf(fpw, "abcdef");
    fclose(fpw);
}
