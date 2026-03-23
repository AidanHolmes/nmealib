#ifndef __CUSTOM_UTILS_H
#define __CUSTOM_UTILS_H

int memorycompare(unsigned char *a, unsigned char *b, unsigned long size);
int stringcompare(unsigned char *a, unsigned char *b, unsigned long max);
void memoryclear(void *a, unsigned long len);
char hexstrtoval(char hex);
char decstrtoval(char dec);
int stringlength(char *str);
void toupper(char *str);
char chartoupper(char c);

#endif