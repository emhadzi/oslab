#include <stdio.h>
#include <stdlib.h>
#define SZ 200000000

int main(){
	FILE *fpw = fopen("fin.txt", "w+");
	for(int i = 0; i < SZ; i++)	
    	fprintf(fpw, "%c", 'a' + (rand() % 26));
    fclose(fpw);
}
//to check number of occurances of some character:
//tr -cd 'a' < fin.txt | wc -c
