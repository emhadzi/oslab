#include <unistd.h>
#include <stdio.h>

void zing(){
	printf("Geia sou %s\n", getlogin());
}
