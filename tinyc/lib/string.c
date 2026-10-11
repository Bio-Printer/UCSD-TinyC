/* string.c -- Tiny-C library: the code behind <string.h> */
#include "libint.h"
size_t strlen(char *s)
{
    char *p;
    p = s;
    while (*p)
        p++;
    return p - s;
}

char *strcpy(char *d, char *s)
{
    char *r;
    r = d;
    while ((*d++ = *s++) != 0)
        ;
    return r;
}

char *strncpy(char *d, char *s, size_t n)
{
    char *r;
    r = d;
    while (n > 0 && *s) {
        *d++ = *s++;
        n--;
    }
    while (n > 0) {
        *d++ = 0;
        n--;
    }
    return r;
}

char *strcat(char *d, char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

char *strncat(char *d, char *s, size_t n)
{
    char *p;
    p = d + strlen(d);
    while (n > 0 && *s) {
        *p++ = *s++;
        n--;
    }
    *p = 0;
    return d;
}

int strcmp(char *a, char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (*a & 255) - (*b & 255);
}

int strncmp(char *a, char *b, size_t n)
{
    while (n > 0 && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (*a & 255) - (*b & 255);
}

char *strchr(char *s, int c)
{
    for (;;) {
        if (*s == (char)c)
            return s;
        if (!*s)
            return NULL;
        s++;
    }
}

char *strrchr(char *s, int c)
{
    char *r;
    r = NULL;
    for (;;) {
        if (*s == (char)c)
            r = s;
        if (!*s)
            return r;
        s++;
    }
}

char *strstr(char *s, char *t)
{
    size_t n;
    n = strlen(t);
    while (*s) {
        if (strncmp(s, t, n) == 0)
            return s;
        s++;
    }
    return n ? NULL : s;
}

void *memcpy(void *d, void *s, size_t n)
{
    if (n > 0)
        __cspv(2, s, 0, d, 0, n);       /* MOVELEFT */
    return d;
}

void *memmove(void *d, void *s, size_t n)
{
    if (n > 0) {
        if ((unsigned)d <= (unsigned)s)
            __cspv(2, s, 0, d, 0, n);   /* MOVELEFT */
        else
            __cspv(3, s, 0, d, 0, n);   /* MOVERIGHT */
    }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    if (n > 0)
        __cspv(10, d, 0, n, c);         /* FILLCHAR */
    return d;
}

int memcmp(void *a, void *b, size_t n)
{
    unsigned char *p;
    unsigned char *q;
    p = a;
    q = b;
    while (n > 0) {
        if (*p != *q)
            return *p - *q;
        p++;
        q++;
        n--;
    }
    return 0;
}

void *memchr(void *s, int c, size_t n)
{
    unsigned char *p;
    int d;
    int k;
    p = s;
    while (n > 0) {
        d = n > 32767 ? 32767 : n;
        k = __cspi(11, d, 0, c & 255, p, 0, 0);     /* SCAN */
        if (k < d)
            return p + k;
        p += d;
        n -= d;
    }
    return NULL;
}

size_t strspn(char *s, char *set)
{
    size_t n;
    n = 0;
    while (s[n] && strchr(set, s[n]))
        n++;
    return n;
}

size_t strcspn(char *s, char *set)
{
    size_t n;
    n = 0;
    while (s[n] && !strchr(set, s[n]))
        n++;
    return n;
}

char *strpbrk(char *s, char *set)
{
    while (*s) {
        if (strchr(set, *s))
            return s;
        s++;
    }
    return NULL;
}

char *__strtok;
char *strtok(char *s, char *delim)
{
    char *r;
    if (s)
        __strtok = s;
    if (!__strtok)
        return NULL;
    __strtok = __strtok + strspn(__strtok, delim);
    if (!*__strtok)
        return NULL;
    r = __strtok;
    __strtok = __strtok + strcspn(__strtok, delim);
    if (*__strtok)
        *__strtok++ = 0;
    return r;
}

char *strdup(char *s)
{
    char *p;
    p = malloc(strlen(s) + 1);
    if (p)
        strcpy(p, s);
    return p;
}

