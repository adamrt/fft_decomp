#ifndef FFT_PSX_LIBC_H
#define FFT_PSX_LIBC_H

extern void srand(unsigned int);
extern int rand(void);
extern int sprintf(char* destination, const char* format, ...);
extern char* strcat(char* destination, const char* source);
extern char* strcpy(char* destination, const char* source);
extern void* memset(void* destination, int value, unsigned int size);
extern void* memcpy(void* destination, const void* source, unsigned int size);
extern void bcopy(const void* source, void* destination, int size);
extern int abs(int value);

#endif
