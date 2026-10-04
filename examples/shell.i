#1 /home/user/UCSD-C/examples/shell.c
 

#1 !/home/user/UCSD-C/tinyc/include/stdio.h
               
#16 !/home/user/UCSD-C/tinyc/include/stdio.h



#1 !/home/user/UCSD-C/tinyc/include/stddef.h
 





typedef unsigned size_t;
typedef int ptrdiff_t;
typedef char wchar_t;


#19 !/home/user/UCSD-C/tinyc/include/stdio.h

#1 !/home/user/UCSD-C/tinyc/include/stdarg.h
     
#6 !/home/user/UCSD-C/tinyc/include/stdarg.h


typedef char *va_list;






#20 !/home/user/UCSD-C/tinyc/include/stdio.h


















typedef struct __file {
    int flags;
    int blk;                     
    int pos;                     
    int len;                     
    int ungot;                   
    int dle;                     
    int linestart;               
    int bufsize;                 
    unsigned char *buf;
    char *fib;                   
} FILE;

extern FILE __files[6 + 3];




int fflush(FILE *f);
int fputc(int c, FILE *f);
int fgetc(FILE *f);

 






 

 




 

FILE *fopen(char *name, char *mode);


int fgetc(FILE *f);

int fputc(int c, FILE *f);

int fflush(FILE *f);

int fclose(FILE *f);

int remove(char *name);

int feof(FILE *f);

int ferror(FILE *f);

void clearerr(FILE *f);

int ungetc(int c, FILE *f);

int fseek(FILE *f, long off, int whence);

long ftell(FILE *f);

void rewind(FILE *f);

size_t fread(void *p, size_t size, size_t n, FILE *f);

size_t fwrite(void *p, size_t size, size_t n, FILE *f);

int getc(FILE *f);

int putc(int c, FILE *f);

int getchar(void);

int putchar(int c);

char *fgets(char *s, int n, FILE *f);

char *gets(char *s);

int fputs(char *s, FILE *f);

int puts(char *s);

void perror(char *s);


 

 



 



 




int vfprintf(FILE *f, char *fmt, va_list ap);

int vsprintf(char *s, char *fmt, va_list ap);

int printf(char *fmt, ...);

int fprintf(FILE *f, char *fmt, ...);

int sprintf(char *s, char *fmt, ...);

 





int vfscanf(FILE *f, char *fmt, va_list ap);

int scanf(char *fmt, ...);

int fscanf(FILE *f, char *fmt, ...);

int sscanf(char *s, char *fmt, ...);

 



#3 /home/user/UCSD-C/examples/shell.c

#1 !/home/user/UCSD-C/tinyc/include/string.h
 



#1 !/home/user/UCSD-C/tinyc/include/stddef.h
 










#5 !/home/user/UCSD-C/tinyc/include/string.h

size_t strlen(char *s);

char *strcpy(char *d, char *s);

char *strncpy(char *d, char *s, size_t n);

char *strcat(char *d, char *s);

char *strncat(char *d, char *s, size_t n);

int strcmp(char *a, char *b);

int strncmp(char *a, char *b, size_t n);

char *strchr(char *s, int c);

char *strrchr(char *s, int c);

char *strstr(char *s, char *t);

void *memcpy(void *d, void *s, size_t n);

void *memmove(void *d, void *s, size_t n);

void *memset(void *d, int c, size_t n);

int memcmp(void *a, void *b, size_t n);

void *memchr(void *s, int c, size_t n);

size_t strspn(char *s, char *set);

size_t strcspn(char *s, char *set);

char *strpbrk(char *s, char *set);

extern char *__strtok;

char *strtok(char *s, char *delim);


char *strdup(char *s);


#4 /home/user/UCSD-C/examples/shell.c

#1 !/home/user/UCSD-C/tinyc/include/psys.h
 



     
#10 !/home/user/UCSD-C/tinyc/include/psys.h


    
#16 !/home/user/UCSD-C/tinyc/include/psys.h
struct crtctrlrec {                  
    char escape, home, eraseeos, eraseeol, ndfs, rlf;
    char backspace;
    unsigned char fillcount;
    char clearline, clearscreen;
    unsigned prefixed;               
};

struct crtinforec {                  
    int height, width;
    char up, down, left, right;
    char eof, flush, brk, stop, chardel, badch;      
    char linedel, altmode;
    char prefix, etx, alpha_lock;
    unsigned prefixed;
};

struct segentry {                    
    int codeunit;
    int diskaddr;                    
    int codeleng;                    
};

struct syscomrec {
    int iorslt;                      
    int xeqerr;                      
    int sysunit;                     
    int bugstate;
    char *gdirp;                     
    int *bombp, *stkbase, *lastmp;   
    int jtab, seg, memtop;
    int bombipc;                     
    int hltline;
    int brkpts[4];
    int retries;
    int expansion[9];
    unsigned lowtime, hightime;      
    unsigned miscinfo;               
    int crttype;
    struct crtctrlrec crtctrl;
    struct crtinforec crtinfo;
    struct segentry segtable[16];    
};

 











      
#78 !/home/user/UCSD-C/tinyc/include/psys.h
int pexec(char *name);

 




 

   
#90 !/home/user/UCSD-C/tinyc/include/psys.h



#5 /home/user/UCSD-C/examples/shell.c

int main(void)
{
    char line[80];
    char here;
    char *heaptop;
    int n;
    if ((((struct syscomrec *)__osvar(1))->expansion[0] == 25603))
        printf("[exit status %d]\n", (((struct syscomrec *)__osvar(1))->expansion[5]));
    for (;;) {
        printf("shell> ");
        if (!fgets(line, sizeof line, (&__files[0])))
            return 0;
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == ' '))
            line[--n] = 0;
        if (n == 0)
            continue;
        if (strcmp(line, "bye") == 0 || strcmp(line, "BYE") == 0)
            return 0;
        if (strcmp(line, "mem") == 0 || strcmp(line, "MEM") == 0) {
            __cspv(32, &heaptop);        
            printf("shell: %u words free\n", (unsigned)(&here - heaptop) / 2);
            continue;
        }
        switch (pexec(line)) {           
        case -1: printf("%s: no such program\n", line); break;
        case -2: printf("%s: not linked\n", line); break;
        default: printf("%s: cannot find the shell's own code file\n", line); break;
        }
    }
}
