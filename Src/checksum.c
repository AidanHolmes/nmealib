#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i=0, len = 0;
    char *str = NULL ;
	short checksum = 0;
	
    if (argc < 2){
        printf("Provide a string as an argument\n");
	}
    
	str = argv[1];
	len = strlen(str);
	
    for (;i < len; i++){
		checksum ^= str[i];
		printf("Val:0x%02X, Checksum 0x%02X\n", str[i], checksum);
	}
	printf("Final checksum 0x%02X\n", checksum);

    return 0;
}