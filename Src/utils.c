#include "utils.h"

int memorycompare(unsigned char *a, unsigned char *b, unsigned long size)
{
	if (!size) return 1; // Succeed if no size given
	while (*a++ == *b++ && --size);
	if (size){
		return 0;
	}
	return 1;
}

int stringcompare(unsigned char *a, unsigned char *b, unsigned long max)
{
	if (!max) return 1; // Succeed if no max size given
	while (*a && *b && *a++ == *b++ && --max);
	if (!(*a) && !(*b)){ // end of string
		return 1;
	}
	return 0 ;
}

void memoryclear(void *a, unsigned long len)
{
	unsigned char *p = a;
	
	while(len--){
		*p++ = 0;
	}
}

char hexstrtoval(char hex)
{
	char out = 0;
	if (hex >= '0' && hex <= '9'){
		out = hex - '0';
	}else if (hex >= 'A' && hex <= 'F'){
		out = 10 + (hex - 'A') ;
	}else if (hex >= 'a' && hex <= 'f'){
		out = 10 + (hex - 'a') ;
	}else{
		out = -1;
	}
	return out;
}

char decstrtoval(char dec)
{
	char out = 0;
	if (dec >= '0' && dec <= '9'){
		out = dec - '0';
	}
	return out;
}

char chartoupper(char c)
{
	if (c >= 'a' && c <= 'z'){
		c -= 32;
	}
	return c;
}

int stringlength(char *str)
{
	int i = 0;
	for (;str[i];i++);
	
	return i;
}

void toupper(char *str)
{
	int i=0;
	for (;str[i];i++){
		str[i] = chartoupper(str[i]);
	}
}