#1 /home/user/UCSD-C/ports/vi/vi.c
 
         
#11 /home/user/UCSD-C/ports/vi/vi.c

              
#26 /home/user/UCSD-C/ports/vi/vi.c


      
#34 /home/user/UCSD-C/ports/vi/vi.c



#1 !/home/user/UCSD-C/tinyc/include/stdarg.h
     
#6 !/home/user/UCSD-C/tinyc/include/stdarg.h


typedef char *va_list;






#37 /home/user/UCSD-C/ports/vi/vi.c

#1 !/home/user/UCSD-C/tinyc/include/string.h
 



#1 !/home/user/UCSD-C/tinyc/include/stddef.h
 





typedef unsigned size_t;
typedef int ptrdiff_t;
typedef char wchar_t;


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


#38 /home/user/UCSD-C/ports/vi/vi.c

#1 !/home/user/UCSD-C/tinyc/include/stdio.h
               
#16 !/home/user/UCSD-C/tinyc/include/stdio.h



#1 !/home/user/UCSD-C/tinyc/include/stddef.h
 










#19 !/home/user/UCSD-C/tinyc/include/stdio.h

#1 !/home/user/UCSD-C/tinyc/include/stdarg.h
     
#6 !/home/user/UCSD-C/tinyc/include/stdarg.h









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

 



#39 /home/user/UCSD-C/ports/vi/vi.c

#1 !/home/user/UCSD-C/tinyc/include/stdlib.h
    
#5 !/home/user/UCSD-C/tinyc/include/stdlib.h



#1 !/home/user/UCSD-C/tinyc/include/stddef.h
 










#8 !/home/user/UCSD-C/tinyc/include/stdlib.h





typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;

 

void *malloc(size_t n);

void free(void *v);

void *calloc(size_t n, size_t size);

void *realloc(void *v, size_t n);

int abs(int x);

long labs(long x);

div_t div(int a, int b);


int rand(void);

void srand(unsigned seed);

long strtol(char *s, char **end, int base);

unsigned long strtoul(char *s, char **end, int base);

int atoi(char *s);

long atol(char *s);

float strtod(char *s, char **end);

float atof(char *s);

double strtold(char *s, char **end);    

double atold(char *s);

char *getenv(char *name);


void qsort(void *base, size_t n, size_t size, int (*cmp)(void *, void *));

void *bsearch(void *key, void *base, size_t n, size_t size, int (*cmp)(void *, void *));

 

void __heapsave(void);

void __heaprestore(void);


void exit(int status);

void abort(void);


#40 /home/user/UCSD-C/ports/vi/vi.c

#1 !/home/user/UCSD-C/tinyc/include/ctype.h
 


int isdigit(int c);
int isupper(int c);
int islower(int c);
int isalpha(int c);
int isalnum(int c);
int isxdigit(int c);
int isspace(int c);
int iscntrl(int c);
int isprint(int c);
int isgraph(int c);
int ispunct(int c);
int toupper(int c);
int tolower(int c);

#41 /home/user/UCSD-C/ports/vi/vi.c

#1 !/home/user/UCSD-C/tinyc/include/conio.h
 



#1 !/home/user/UCSD-C/tinyc/include/stdio.h
               
#16 !/home/user/UCSD-C/tinyc/include/stdio.h

























































































































































#5 !/home/user/UCSD-C/tinyc/include/conio.h

int getch(void);

int getche(void);

int putch(int c);

int cputs(char *s);

 
int kbhit(void);

 
void gotoxy(int x, int y);

void clrscr(void);


#42 /home/user/UCSD-C/ports/vi/vi.c

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

 











         
#81 !/home/user/UCSD-C/tinyc/include/psys.h
int pexec(char *cmd);

 



   
#90 !/home/user/UCSD-C/tinyc/include/psys.h


int __pxsum(unsigned char *s);

 

   
#99 !/home/user/UCSD-C/tinyc/include/psys.h


    
#105 !/home/user/UCSD-C/tinyc/include/psys.h




          
#119 !/home/user/UCSD-C/tinyc/include/psys.h
unsigned memfill(void);
unsigned memgap(void);


#43 /home/user/UCSD-C/ports/vi/vi.c


































struct globals G;

typedef signed char smallint;
































































































#179 /home/user/UCSD-C/ports/vi/vi.c




#1 !/home/user/UCSD-C/tinyc/include/limits.h
 


















#183 /home/user/UCSD-C/ports/vi/vi.c

















 














enum {
	MAX_TABSTOP = 32, 
	
	
	MAX_INPUT_LEN = 128,
	
	MAX_SCR_COLS = 132,
	MAX_SCR_ROWS = 132,
};

























 
 


static const char SOs[]  = "\017\017\017\017";
static const char SOn[]  = "\016\016\016\016";




 
static const char bell[]  = "\007";
 
static const char Ceol[]  = "\033[0K";
static const char Ceos[]  = "\033[0J";
 
static const char CMrc[]  = "\033[%d;%dH";











enum {
	YANKONLY = 0,
	YANKDEL = 1,
	FORWARD = 1,	
	BACK = -1,	
	LIMITED = 0,	
	FULL = 1,	

	S_BEFORE_WS = 1,	
	S_TO_WS = 2,		
	S_OVER_WS = 3,		
	S_END_PUNCT = 4,	
	S_END_ALNUM = 5,	
};

enum {  
	CMODE_COMMAND,
	CMODE_INSERT,
	CMODE_REPLACE,
	CMODES,
	CMODE_LINE_INPUT = 1<<4
};

static const char *cmd_mode_indicator[] =
	{"COMMAND", "INSERT", "REPLACE", "?!?" };

 
 
 

struct globals {
	 
	char *text, *end;       
	char *dot;              
	int text_size;		

	 
	smallint vi_setops;







 



	smallint readonly_mode;









	smallint editing;        
	                         
	smallint cmd_mode;       
	int file_modified;       
	int last_file_modified;  
	int fn_start;            
	int save_argc;           
	int cmdcnt;              
	unsigned rows, columns;	 
	int crow, ccol;          
	int offset;              
	char *current_filename;
	char *screenbegin;       
	char *screen;            
	int screensize;          
	int tabstop;
	char erase_char;         
	char last_input_char;    
	char last_forward_char;  


	smallint adding2q;	 
	int lmc_len;             
	char *ioq, *ioq_start;   








	char *modifying_cmds;    


	char *last_search_pattern; 

	int chars_to_parse;
	 

	char *edit_file__cur_line;

	int refresh__old_offset;
	int format_edit_status__tot;

	 

	int YDreg, Ureg;        
	char *reg[28];          
	char *mark[28];         
	char *context_start, *context_end;







	unsigned ticsPerChar;	

	char *initial_cmds[3];  

	
	
	char readbuffer[128];

	char status_buffer[200]; 
	char displayed_buffer[200];  

	char last_modifying_cmd[MAX_INPUT_LEN];	

	char get_input_line__buf[MAX_INPUT_LEN];  

	char scr_out_buf[MAX_SCR_COLS + MAX_TABSTOP * 2];
};































































static int init_text_buffer(char *); 
static void edit_file(char *);	
static void do_cmd(char);	
static int next_tabstop(int);
static void sync_cursor(char *, int *, int *);	
static char *begin_line(char *);	
static char *end_line(char *);	
static char *prev_line(char *);	
static char *next_line(char *);	
static char *end_screen(void);	
static int count_lines(char *, char *);	
static char *find_line(int);	
static char *move_to_col(char *, int);	
static void dot_left(void);	
static void dot_right(void);	
static void dot_begin(void);	
static void dot_end(void);	
static void dot_next(void);	
static void dot_prev(void);	
static void dot_scroll(int, int);	
static void dot_skip_over_ws(void);	
static void dot_delete(void);	
static char *bound_dot(char *);	
static char *new_screen(int, int);	
static char *char_insert(char *, char);	
static char *stupid_insert(char *, char);	
static int find_range(char **, char **, char);	
static int st_test(char *, int, int, char *);	
static char *skip_thing(char *, int, int, int);	
static char *find_pair(char *, char);	
static char *text_hole_delete(char *, char *);	
static char *text_hole_make(char *, int);	
static char *yank_delete(char *, char *, int, int);	
static void show_help(void);	
static int rawmode(void);	
static void cookmode(void);	
static int awaitInput(int); 
static char readit(void);	
static char get_one_char(void);	
static int file_size(const char *);   

static int file_insert(const char *, char *, int);



static int file_write(char *, char *, char *);



static void place_cursor(int, int);
static void screen_erase(void);
static void clear_to_eol(void);
static void clear_to_eos(void);
static void standout_start(void);	
static void standout_end(void);	
static void flash(int);		
static void show_status_line(void);	
static void status_line(const char *, ...);     
static void status_line_bold(const char *, ...);
static void not_implemented(const char *); 
static int format_edit_status(const char *fmt); 
static void redraw(void);	
static char* format_line(char*  );
static void refresh(void);	

static void Indicate_Error(void);       

static void Hit_Return(void);


static char *char_search(char *, const char *, int, int);	
static int mycmp(const char *, const char *, int);	


static char *get_one_address(char *, int *);	
static char *get_address(char *, int *, int *);	
static void colon(char *);	








static void start_new_cmd_q(char);	
static void end_cmd_q(void);	




static void showmatching(char *);	


static char *string_insert(char *, char *);	


static char *text_yank(char *, char *, int);	
static char what_reg(void);		
static void check_context(char);	





#1 !/home/user/UCSD-C/ports/vi/viucsd.h
   
#4 !/home/user/UCSD-C/ports/vi/viucsd.h
 
static void gracefulExit(void);

static void gracefulExit(void);

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr) return ptr;
	gracefulExit();
	printf("vi: out of memory (%u bytes)\n", size);
	exit(65);
	return ((void *)0);
}

void *xzalloc(size_t size)
{
	return memset(xmalloc(size), 0, size);
}

void *xstrdup(const char *s)
{
	return strcpy(xmalloc(strlen(s) + 1), s);
}

void *xstrndup(const char *s, size_t n)
{
	char *p;
	size_t l;
	for (l = 0; l < n && s[l]; l++)
		;
	p = xmalloc(l + 1);
	memcpy(p, s, l);
	p[l] = 0;
	return p;
}

void *xrealloc(void *old, size_t size)
{
	void *ptr = realloc(old, size);
	if (ptr) return ptr;
	gracefulExit();
	printf("vi: out of memory (%u bytes)\n", size);
	exit(68);
	return ((void *)0);
}



char* last_char_is(const char *s, int c)
{
	if (s && *s) {
		s += strlen(s) - 1;
		if ((unsigned char)*s == c)
			return (char*)s;
	}
	return ((void *)0);
}

int bb_putchar(int ch)
{
	return putc(ch, (&__files[1]));
}

char *strchrnul(const char *s, int c)
{
	while (*s && *s != (char)c)
		s++;
	return (char *)s;
}

void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char *cp = (const unsigned char *)s + n;
	while (n--)
		if (*--cp == (unsigned char)c)
			return (void *)cp;
	return ((void *)0);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
	int d;
	for (; n; n--, a++, b++) {
		d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
		if (d || !*a)
			return d;
	}
	return 0;
}

static char *strerror(int e)
{
	return "I/O error";
}


 
static int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	static char tmp[300];
	va_list ap;
	int len;
	((ap) = __va_start());
	len = vsprintf(tmp, fmt, ap);
	((void)0);
	if (n) {
		if ((size_t)len < n) {
			memcpy(buf, tmp, len + 1);
		} else {
			memcpy(buf, tmp, n - 1);
			buf[n - 1] = 0;
		}
	}
	return len;
}

 
static int optind = 1;
static char *optarg;
static int getopt(int argc, char **argv, const char *opts)
{
	static int sp = 1;
	char *o;
	int c;
	if (sp == 1 && (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == 0))
		return -1;
	c = argv[optind][sp];
	o = strchr(opts, c);
	if (!o || c == ':') {
		if (!argv[optind][++sp]) {
			optind++;
			sp = 1;
		}
		return '?';
	}
	if (o[1] == ':') {
		if (argv[optind][sp + 1])
			optarg = argv[optind] + sp + 1;
		else if (optind + 1 < argc)
			optarg = argv[++optind];
		else
			optarg = "";
		optind++;
		sp = 1;
	} else if (!argv[optind][++sp]) {
		optind++;
		sp = 1;
	}
	return c;
}

 
static void show_help(void)
{
	static const char *help[] = {
		"These features are available:",
		"\tPattern searches with / and ?",
		"\tLast command repeat with \'.\'",
		"\tLine marking with 'x",
		"\tNamed buffers with \"x",
		"\tReadonly if vi is called as \"view\"",
		"\tReadonly with -R command line arg",
		"\tSome colon mode commands with \':\'",
		"\tSettable options with \":set\"",
		0
	};
	int i;
	for (i = 0; help[i]; i++)
		puts(help[i]);
}

 
    
#181 !/home/user/UCSD-C/ports/vi/viucsd.h
static char *ucsd_res[2];

static void ucsd_reserve(void)
{
	if (!ucsd_res[0])
		ucsd_res[0] = malloc(1024);
	if (!ucsd_res[1])
		ucsd_res[1] = malloc(80);
}

static void ucsd_unreserve(void)
{
	free(ucsd_res[1]);
	free(ucsd_res[0]);
	ucsd_res[0] = ucsd_res[1] = ((void *)0);
}

 
static char *ucsd_askname(void)
{
	static char name[40];
	int n;
	printf("Edit what file? ");
	fflush((&__files[1]));
	if (!fgets(name, sizeof(name), (&__files[0])))
		name[0] = 0;
	n = strlen(name);
	while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == ' '))
		name[--n] = 0;
	return name;
}

  
#215 !/home/user/UCSD-C/ports/vi/viucsd.h
static int rawmode(void)
{
	ucsd_reserve();
	(G.erase_char         ) = ((struct syscomrec *)__osvar(1))->crtinfo.chardel;
	(G.ticsPerChar   ) = 1;
	return 0;
}

static void cookmode(void)
{
}

  
#229 !/home/user/UCSD-C/ports/vi/viucsd.h
static int awaitInput(int tics)
{
	unsigned start;
	long polls;
	fflush((&__files[1]));
	if (kbhit())
		return 1;
	if (tics <= 0)
		return 0;
	if (((struct syscomrec *)__osvar(1))->miscinfo & 0x0001) {
		start = ((struct syscomrec *)__osvar(1))->lowtime;
		while ((unsigned)(((struct syscomrec *)__osvar(1))->lowtime - start) < (unsigned)(tics * 6 / 10 + 1))
			if (kbhit())
				return 1;
		return 0;
	}
	for (polls = tics * 20L; polls > 0; polls--)
		if (kbhit())
			return 1;
	return 0;
}

  
#253 !/home/user/UCSD-C/ports/vi/viucsd.h
static char readit(void)
{
	int c;
	struct crtinforec *ci;
	if ((G.chars_to_parse     ) > 0) {
		c = (G.readbuffer    )[0];
		(G.chars_to_parse     )--;
		memmove((G.readbuffer    ), (G.readbuffer    ) + 1, (G.chars_to_parse     ));
		return c;
	}
	fflush((&__files[1]));
	c = getch();
	ci = &((struct syscomrec *)__osvar(1))->crtinfo;
	if (c == ci->up)
		return (char)128;
	if (c == ci->down)
		return (char)129;
	if (c == ci->left)
		return (char)131;
	if (c == ci->right)
		return (char)130;
	return c;
}

  
#279 !/home/user/UCSD-C/ports/vi/viucsd.h
static int file_size(const char *fn)
{
	FILE *f;
	long n;
	int c;
	if (!fn || !fn[0])
		return -1;
	ucsd_unreserve();
	f = fopen(fn, "r");
	n = 0;
	if (f) {
		while ((c = getc(f)) != (-1))
			n++;
		fclose(f);
	}
	ucsd_reserve();
	if (!f)
		return -1;
	if (n > 30000L) {	 
		gracefulExit();
		printf("vi: %s is too big (%ld characters, at most 30000)\n", fn, n);
		exit(1);
	}
	return (int)n;
}

static int file_insert(const char *fn, char *p, int update_ro_status)
{
	FILE *f;
	int cnt, size, c;

	if (p < (G.text          ) || p > (G.end           )) {
		status_line_bold("Trying to insert file outside of memory");
		return -1;
	}
	size = file_size(fn);
	if (size < 0) {
		status_line_bold("\"%s\" cannot open", fn);
		return -1;
	}
	p = text_hole_make(p, size);
	ucsd_unreserve();
	f = fopen(fn, "r");
	cnt = 0;
	if (f) {
		for (; cnt < size && (c = getc(f)) != (-1); cnt++)
			p[cnt] = c;
		fclose(f);
	}
	ucsd_reserve();
	if (cnt < size) {
		text_hole_delete(p + cnt, p + size - 1);
		status_line_bold("cannot read all of file \"%s\"", fn);
	} else
		(G.file_modified      )++;
	return cnt;
}

static int file_write(char *fn, char *first, char *last)
{
	FILE *f;
	int cnt;
	char *p;

	if (fn == 0) {
		status_line_bold("No current filename");
		return -2;
	}
	ucsd_unreserve();
	f = fopen(fn, "w");
	if (!f) {
		ucsd_reserve();
		return -1;
	}
	cnt = last - first + 1;
	for (p = first; p <= last; p++)
		if (putc(*p, f) == (-1))
			break;
	if (fclose(f) == (-1) || p <= last)
		cnt = 0;
	ucsd_reserve();
	return cnt;
}

static void place_cursor(int row, int col)
{
	if (row < 0) row = 0;
	if (row >= (G.rows               )) row = (G.rows               ) - 1;
	if (col < 0) col = 0;
	if (col >= (G.columns            )) col = (G.columns            ) - 1;
	fflush((&__files[1]));
	gotoxy(col, row);
}

static void clear_to_eol(void)
{
	putc(((struct syscomrec *)__osvar(1))->crtctrl.eraseeol, (&__files[1]));
}

static void clear_to_eos(void)
{
	putc(((struct syscomrec *)__osvar(1))->crtctrl.eraseeos, (&__files[1]));
	*(G.displayed_buffer   )=0;   
}
#579 /home/user/UCSD-C/ports/vi/vi.c



































































































































static void write1(const char *out)
{
	fputs(out, (&__files[1]));
}

static void clear_screen(void)
{
	place_cursor(0, 0);	
	clear_to_eos();		
}

static void gracefulExit(void)
{
	cookmode();
	place_cursor((G.rows               )-1, 0);	
	clear_to_eol();		
	fflush((&__files[1]));
}

void clampScreenSize(void)
{
	if ((G.rows               ) < 2)
		(G.rows               ) = 2;
	else if ((G.rows               ) > MAX_SCR_ROWS)
		(G.rows               ) = MAX_SCR_ROWS;
	if ((G.columns            ) < 2)
		(G.columns            ) = 2;
	else if ((G.columns            ) > MAX_SCR_COLS)
		(G.columns            ) = MAX_SCR_COLS;
}



















































































static void createScreen(void)
{




	new_screen((G.rows               ), (G.columns            ));	
}


int main(int argc, char **argv) ;
int main(int argc, char **argv)
{
	int c;

	;
	(G.rows               ) = 24;
	(G.columns            ) = 80;

	if (((struct syscomrec *)__osvar(1))->crtinfo.height > 0)
		(G.rows               ) = ((struct syscomrec *)__osvar(1))->crtinfo.height;
	if (((struct syscomrec *)__osvar(1))->crtinfo.width > 0)
		(G.columns            ) = ((struct syscomrec *)__osvar(1))->crtinfo.width;


	{  
		char *txt = getenv("LINES");
		if (txt)
			(G.rows               ) = atoi(txt);
		txt = getenv("COLUMNS");
		if (txt)
			(G.columns            ) = atoi(txt);
		clampScreenSize();
	}












	(G.vi_setops          ) = 1 | 2 | 4;

	(G.modifying_cmds     ) = "aAcCdDiIJoOpPrRsxX<>~";	


	
	
	

	{
		char *p = getenv("EXINIT");
		if (p && *p)
			(G.initial_cmds  )[0] = xstrndup(p, MAX_INPUT_LEN);
	}

	while ((c = getopt(argc, argv, "hCRH-" "c:")) != -1) {
		switch (c) {

		case 'R':		
			(((G.readonly_mode      )) |= 0x02);
			break;


		case 'c':		
			if (*optarg)
				(G.initial_cmds  )[(G.initial_cmds  )[0] != 0] = xstrndup(optarg, MAX_INPUT_LEN);
			break;

		case 'H':
		case '-':
			show_help();
			 
		default:
			;
			return 1;
		}
	}

	
	
	(G.fn_start           ) = optind;	
	(G.save_argc          ) = argc;

	
	if (optind >= argc) {

		char *name = ucsd_askname();
		edit_file(*name ? name : 0);



	} else {
		for (; optind < argc; optind++) {
			edit_file(argv[optind]);
		}
	}
	

	return 0;
}

 
 
static int init_text_buffer(char *fn)
{
	int rc;
	int size = file_size(fn);	

	 
	free((G.text          ));
	(G.text_size     ) = size + 1024;
	(G.screenbegin        ) = (G.dot           ) = (G.end           ) = (G.text          ) = xzalloc((G.text_size     ));

	if (fn != (G.current_filename   )) {
		free((G.current_filename   ));
		(G.current_filename   ) = xstrdup(fn);
	}
	if (size < 0) {
		
		char_insert((G.text          ), '\n');
		rc = 0;
	} else {
		rc = file_insert(fn, (G.text          ), 1);
	}
	(G.file_modified      ) = 0;
	(G.last_file_modified ) = -1;

	 
	memset((G.mark          ), 0, sizeof((G.mark          )));

	return rc;
}

static void edit_file(char *fn)
{



	char c;
	(G.editing            ) = 1;	
	if (rawmode()) {
		perror("vi");
		exit(5);
	}
	createScreen();
	init_text_buffer(fn);


	(G.YDreg         ) = 26;			
	(G.Ureg          ) = 27;			
	(G.mark          )[26] = (G.mark          )[27] = (G.text          );	


	(G.last_forward_char  ) = (G.last_input_char    ) = '\0';
	(G.crow               ) = 0;
	(G.ccol               ) = 0;
	(G.tabstop            ) = 8;
	(G.offset             ) = 0;			
	clear_screen();
















	(G.cmd_mode           ) = CMODE_COMMAND;
	(G.cmdcnt             ) = 0;
	c = '\0';

	free((G.ioq_start          ));
	(G.ioq                ) = (G.ioq_start          ) = ((void *)0);
	(G.lmc_len            ) = 0;
	(G.adding2q           ) = 0;



	{
		char *p, *q;
		int n = 0;

		while ((p = (G.initial_cmds  )[n])) {
			do {
				q = p;
				p = strchr(q, '\n');
				if (p)
					while (*p == '\n')
						*p++ = '\0';
				if (*q)
					colon(q);
			} while (p);
			free((G.initial_cmds  )[n]);
			(G.initial_cmds  )[n] = ((void *)0);
			n++;
		}
	}


	
	while ((G.editing            ) > 0) {
		refresh();
		(G.last_input_char    ) = c = get_one_char();	
		*(G.status_buffer      )=0;

		
		if (begin_line((G.dot           )) != (G.edit_file__cur_line)) {
			(G.edit_file__cur_line) = begin_line((G.dot           ));
			text_yank(begin_line((G.dot           )), end_line((G.dot           )), (G.Ureg          ));
		}


		
		
		if (!(G.adding2q           ) && (G.ioq_start          ) == ((void *)0) 		 && strchr((G.modifying_cmds     ), c) 		) {
#1054 /home/user/UCSD-C/ports/vi/vi.c
			start_new_cmd_q(c);
		}

		do_cmd(c);		
	}
	
	refresh();
	gracefulExit();

}



static char *get_one_address(char *p, int *addr)	
{
	int st;
	char *q;
	char c;
	char *pat;

	*addr = -1;			
	if (*p == '.') {	
		p++;
		q = begin_line((G.dot           ));
		*addr = count_lines((G.text          ), q);
	}

	else if (*p == '\'') {	
		p++;
		c = tolower(*p);
		p++;
		if (c >= 'a' && c <= 'z') {
			
			c = c - 'a';
			q = (G.mark          )[(unsigned char) c];
			if (q != ((void *)0)) {	
				*addr = count_lines((G.text          ), q);	
			}
		}
	}


	else if (*p == '/') {	
		q = strchrnul(++p, '/');
		pat = xstrndup(p, q - p); 
		p = q;
		if (*p == '/')
			p++;
		q = char_search((G.dot           ), pat, FORWARD, FULL);
		if (q != ((void *)0)) {
			*addr = count_lines((G.text          ), q);
		}
		free(pat);
	}

	else if (*p == '$') {	
		p++;
		q = begin_line((G.end           ) - 1);
		*addr = count_lines((G.text          ), q);
	} else if (((unsigned)((*p) - '0') <= 9)) {	
		sscanf(p, "%d%n", addr, &st);
		p += st;
	} else {
		
		*addr = -1;
	}
	return p;
}

static char *get_address(char *p, int *b, int *e)	
{
	
	
	while (((*p) == ' ' || (*p) == '\t'))
		p++;				
	if (*p == '%') {			
		p++;
		*b = 1;
		*e = count_lines((G.text          ), (G.end           )-1);
		goto ga0;
	}
	p = get_one_address(p, b);
	while (((*p) == ' ' || (*p) == '\t'))
		p++;
	if (*p == ',') {			
		p++;
		while (((*p) == ' ' || (*p) == '\t'))
			p++;
		
		p = get_one_address(p, e);
	}
 ga0:
	while (((*p) == ' ' || (*p) == '\t'))
		p++;				
	return p;
}


static void setops(const char *args, const char *opname, int flg_no, 			const char *short_opname, int opt)
#1154 /home/user/UCSD-C/ports/vi/vi.c
{
	const char *a = args + flg_no;
	int l = strlen(opname) - 1;  

	if (strncasecmp(a, opname, l) == 0 	 || strncasecmp(a, short_opname, 2) == 0 	) {
#1161 /home/user/UCSD-C/ports/vi/vi.c
		if (flg_no)
			(G.vi_setops          ) &= ~opt;
		else
			(G.vi_setops          ) |= opt;
	}
}




static void showmatching(char *p)
{
	char *q, *save_dot;

	
	q = find_pair(p, *p);	
	if (q == ((void *)0)) {
		Indicate_Error();	
	} else {
		
		save_dot = (G.dot           );	
		(G.dot           ) = q;		
		refresh();		
		awaitInput(40);	
		(G.dot           ) = save_dot;	
		refresh();
	}
}

static char *stpcopy(char *dest, const char *src)
{
	while(*src) *dest++ = *src++;
	*dest = 0;
	return dest;
}






static void colon_set(char *args)
{
	int i, ch;
	{
		char *argp = args;
		while (*argp) {
		  i = 0;			
			if (strncasecmp(argp, "no", 2) == 0)
				i = 2;		
			setops(argp, "autoindent ", i, "ai", 1);
			setops(argp, "flash ", i, "fl", 8);
			setops(argp, "ignorecase ", i, "ic", 4);
			setops(argp, "showmatch ", i, "sm", 2);
			 
			if (strncasecmp(argp + i, "tabstop=%d ", 7) == 0) {
				sscanf(strchr(argp + i, '='), "tabstop=%d" + 7, &ch);
				if (ch > 0 && ch <= MAX_TABSTOP)
					(G.tabstop            ) = ch;
			}
			while (*argp && *argp != ' ')
				argp++; 
			while (*argp && *argp == ' ')
				argp++; 
		}
		
		char *cursor = (G.status_buffer      );
		*cursor = 0;
		if (!((G.vi_setops          ) & 1))
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"autoindent ");
		if (!((G.vi_setops          ) & 8))
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"flash ");
		if (!((G.vi_setops          ) & 4))
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"ignorecase ");
		if (!((G.vi_setops          ) & 2 ))
			cursor = stpcopy(cursor,"no");
		cursor = stpcopy(cursor,"showmatch ");
		cursor += sprintf(cursor,"tabstop=%d ", (G.tabstop            ));
	}
}




static int colon_s(char *orig_buf, char *q, int b, int e)
{
	{
		char *ls, *F, *R, *buf1;
		int gflag, i;
		char c;

		
		
		
		gflag = 0;		
		c = orig_buf[1];	
		F = orig_buf + 2;	
		R = strchr(F, c);	
		if (!R) {
			status_line_bold(":s expression missing delimiters");
			return 1;
		}
		if (R == F) { 
			if (!(F = (G.last_search_pattern)))
			{
				status_line_bold("No previous regular expression");
				return 1;
			}
			F++;  
		}
		*R++ = '\0';	
		buf1 = strchr(R, c);
		if (buf1) {  
			*buf1++ = '\0';	
			if (*buf1 == 'g') {	
				buf1++;
				gflag++;	
			}
		}
		q = begin_line(q);
		if (b < 0) {	
			q = begin_line((G.dot           ));	
			b = count_lines((G.text          ), q);	
		}
		if (e < 0)
			e = b;		
		for (i = b; i <= e; i++) {	
			ls = q;		
vc4:
			buf1 = char_search(q, F, FORWARD, LIMITED);	
			if (buf1) {
				
				text_hole_delete(buf1, buf1 + strlen(F) - 1);
				
				string_insert(buf1, R);	
				
				if (gflag == 1) {
					if ((buf1 + strlen(R)) < end_line(ls)) {
						q = buf1 + strlen(R);
						goto vc4;	
					}
				}
			}
			q = next_line(ls);
		}
	}
	return 0;
}

static void colon(char *buf)
{
	char c, *orig_buf, *buf1, *q, *r;
	char *fn, cmd[MAX_INPUT_LEN], args[MAX_INPUT_LEN];
	int i, l, li, ch, b, e;
	int useforce, forced = 0;

	
	
	
	
	
	
	
	
	
	
	
	
	
	

	if (!buf[0])
		goto vc1;
	if (*buf == ':')
		buf++;			

	li = ch = i = 0;
	b = e = -1;
	q = (G.text          );			
	r = (G.end           ) - 1;
	li = count_lines((G.text          ), (G.end           ) - 1);
	fn = (G.current_filename   );

	
	buf = get_address(buf, &b, &e);

	
	orig_buf = buf;

	
	buf1 = cmd;
	while (*buf != '\0') {
		if (isspace(*buf))
			break;
		*buf1++ = *buf++;
	}
	*buf1 = '\0';
	
	while (((*buf) == ' ' || (*buf) == '\t'))
		buf++;
	strcpy(args, buf);
	useforce = 0;
	buf1 = last_char_is(cmd, '!');
	if (buf1) {
		useforce = 1;
		*buf1 = '\0';   
	}
	if (b >= 0) {
		
		
		
		
		q = find_line(b);	
		r = end_line(q);
		li = 1;
	}
	if (e >= 0) {
		
		
		r = find_line(e);	
		r = end_line(r);
		li = e - b + 1;
	}
	
	i = strlen(cmd);
	if (i == 0) {		
		if (b >= 0) {
			(G.dot           ) = find_line(b);	
			dot_skip_over_ws();
		}
	}

















	else if (strncmp(cmd, "=", i) == 0) {	
		if (b < 0) {	
			b = e = count_lines((G.text          ), (G.dot           ));
		}
		status_line("%d", b);
	} else if (strncasecmp(cmd, "delete", i) == 0) {	
		if (b < 0) {	
			q = begin_line((G.dot           ));	
			r = end_line((G.dot           ));
		}
		(G.dot           ) = yank_delete(q, r, 1, YANKDEL);	
		dot_skip_over_ws();
	} else if (strncasecmp(cmd, "edit", i) == 0) {	
		
		if ((G.file_modified      ) && !useforce) {
			status_line_bold("No write since last change (:edit! overrides)");
			goto vc1;
		}
		if (args[0]) {
			
			fn = args;
		} else if ((G.current_filename   ) && (G.current_filename   )[0]) {
			
			
		} else {
			
			status_line_bold("No current filename");
			goto vc1;
		}

		if (init_text_buffer(fn) < 0)
			goto vc1;


		if ((G.Ureg          ) >= 0 && (G.Ureg          ) < 28 && (G.reg           )[(G.Ureg          )] != 0) {
			free((G.reg           )[(G.Ureg          )]);	
			(G.reg           )[(G.Ureg          )]= 0;
		}
		if ((G.YDreg         ) >= 0 && (G.YDreg         ) < 28 && (G.reg           )[(G.YDreg         )] != 0) {
			free((G.reg           )[(G.YDreg         )]);	
			(G.reg           )[(G.YDreg         )]= 0;
		}

		
		li = count_lines((G.text          ), (G.end           ) - 1);
		status_line("\"%s\"%s" 			"%s" 			" %dL, %dC", (G.current_filename   ), 			(file_size(fn) < 0 ? " [New file]" : ""), 			(((G.readonly_mode      )) ? " [Readonly]" : ""), 			li, ch);
#1463 /home/user/UCSD-C/ports/vi/vi.c
	} else if (strncasecmp(cmd, "file", i) == 0) {	
		if (b != -1 || e != -1) {
			not_implemented("No address allowed on this command");
			goto vc1;
		}
		if (args[0]) {
			
			free((G.current_filename   ));
			(G.current_filename   ) = xstrdup(args);
		}
	} else if (strncasecmp(cmd, "features", i) == 0) {	
		
		place_cursor((G.rows               ) - 1, 0);	
		clear_to_eol();	
		cookmode();
		show_help();
		rawmode();
		Hit_Return();
	} else if (strncasecmp(cmd, "list", i) == 0) {	
		if (b < 0) {	
			q = begin_line((G.dot           ));	
			r = end_line((G.dot           ));
		}
		place_cursor((G.rows               ) - 1, 0);	
		clear_to_eol();	
		puts("\r");
		for (; q <= r; q++) {
			int c_is_no_print;

			c = *q;
			c_is_no_print = (c & 0x80) && !((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f);
			if (c_is_no_print) {
				c = '.';
				standout_start();
			}
			if (c == '\n') {
				write1("$\r");
			} else if (c < ' ' || c == 127) {
				bb_putchar('^');
				if (c == 127)
					c = '?';
				else
					c += '@';
			}
			bb_putchar(c);
			if (c_is_no_print)
				standout_end();
		}
		Hit_Return();
	} else if (strncasecmp(cmd, "quit", i) == 0  	        || strncasecmp(cmd, "next", i) == 0  	) {
#1515 /home/user/UCSD-C/ports/vi/vi.c
		if (useforce) {
			
			if (*cmd == 'q') {
				optind = (G.save_argc          );
			}
			(G.editing            ) = 0;
			goto vc1;
		}
		
		if ((G.file_modified      )) {
			status_line_bold("No write since last change (:%s! overrides)", 				 (*cmd == 'q' ? "quit" : "next"));
#1527 /home/user/UCSD-C/ports/vi/vi.c
			goto vc1;
		}
		
		if (*cmd == 'q' && optind < (G.save_argc          ) - 1) {
			status_line_bold("%d more file to edit", ((G.save_argc          ) - optind - 1));
			goto vc1;
		}
		if (*cmd == 'n' && optind >= (G.save_argc          ) - 1) {
			status_line_bold("No more files to edit");
			goto vc1;
		}
		(G.editing            ) = 0;
	} else if (strncasecmp(cmd, "read", i) == 0) {	
		fn = args;
		if (!fn[0]) {
			status_line_bold("No filename given");
			goto vc1;
		}
		if (b < 0) {	
			q = begin_line((G.dot           ));	
		}
		
		if (b != 0)
			q = next_line(q);
		ch = file_insert(fn, q, 0);
		if (ch < 0)
			goto vc1;	
		
		li = count_lines(q, q + ch - 1);
		status_line("\"%s\"" 			"%s" 			" %dL, %dC", fn, 			((G.readonly_mode      ) ? " [Readonly]" : ""), 			li, ch);
#1561 /home/user/UCSD-C/ports/vi/vi.c
		if (ch > 0) {
			
			if (q <= (G.dot           ))
				(G.dot           ) += ch;
			(G.file_modified      )++;
		}
	} else if (strncasecmp(cmd, "rewind", i) == 0) {	
		if ((G.file_modified      ) && !useforce) {
			status_line_bold("No write since last change (:rewind! overrides)");
		} else {
			
			optind = (G.fn_start           ) - 1;
			(G.editing            ) = 0;
		}

	} else if (strncasecmp(cmd, "set", i) == 0) {	

		colon_set(args);



	} else if (strncasecmp(cmd, "s", 1) == 0) {	
		if (colon_s(orig_buf, q, b, e))
			return;

	} else if (strncasecmp(cmd, "version", i) == 0) {  
		status_line("version 2.63 (UCSD)" " " "brent@mbari.org");
	} else if (strncasecmp(cmd, "write", i) == 0   	        || strncasecmp(cmd, "wq", i) == 0 	        || strncasecmp(cmd, "wn", i) == 0 	        || strncasecmp(cmd, "x", i) == 0 	) {
#1593 /home/user/UCSD-C/ports/vi/vi.c
		
		if (args[0]) {
			fn = args;
		}

		if ((G.readonly_mode      ) && !useforce) {
			status_line_bold("\"%s\" File is read only", fn);
			goto vc3;
		}

		
		li = count_lines(q, r);
		ch = r - q + 1;
		
		if (useforce) {
			
			
			
			forced = 1;
		}
		l = file_write(fn, q, r);
		if (useforce && forced) {
			
			
			
			forced = 0;
		}
		if (l < 0) {
			if (l == -1)
				status_line_bold("\"%s\" %s", fn, strerror((((struct syscomrec *)__osvar(1))->iorslt)));
		} else {
			status_line("\"%s\" %dL, %dC", fn, li, l);
			if (q == (G.text          ) && r == (G.end           ) - 1 && l == ch) {
				(G.file_modified      ) = 0;
				(G.last_file_modified ) = -1;
			}
			if ((cmd[0] == 'x' || cmd[1] == 'q' || cmd[1] == 'n' || 			     cmd[0] == 'X' || cmd[1] == 'Q' || cmd[1] == 'N') 			     && l == ch) {
#1632 /home/user/UCSD-C/ports/vi/vi.c
				(G.editing            ) = 0;
			}
		}

 vc3:;


	} else if (strncasecmp(cmd, "yank", i) == 0) {	
		if (b < 0) {	
			q = begin_line((G.dot           ));	
			r = end_line((G.dot           ));
		}
		text_yank(q, r, (G.YDreg         ));
		li = count_lines(q, r);
		status_line("Yank %d lines (%d chars) into [%c]", 				li, strlen((G.reg           )[(G.YDreg         )]), what_reg());
#1648 /home/user/UCSD-C/ports/vi/vi.c

	} else {
		
		not_implemented(cmd);
	}
 vc1:
	(G.dot           ) = bound_dot((G.dot           ));	
	return;

colon_s_fail:
	status_line_bold(":s expression missing delimiters");
	return;
colon_no_regex:
	status_line_bold ("No previous regular expression");

}



static void Hit_Return(void)
{
	char c;

	standout_start();
	write1("[Hit return to continue]");
	standout_end();
	while ((c = get_one_char()) != '\n' && c != '\r' && c != 27)
		continue;
	redraw();
}

static int next_tabstop(int col)
{
	return col + (((G.tabstop            ) - 1) - (col % (G.tabstop            )));
}


static void sync_cursor(char *d, int *row, int *col)
{
	char *beg_cur;	
	char *tp;
	int cnt, ro, co;

	beg_cur = begin_line(d);	

	if (beg_cur < (G.screenbegin        )) {
		
		
		cnt = count_lines(beg_cur, (G.screenbegin        ));
 sc1:
		(G.screenbegin        ) = beg_cur;
		if (cnt > ((G.rows               ) - 1) / 2) {
			
			for (cnt = 0; cnt < ((G.rows               ) - 1) / 2; cnt++) {
				(G.screenbegin        ) = prev_line((G.screenbegin        ));
			}
		}
	} else {
		char *end_scr;	
		end_scr = end_screen();	
		if (beg_cur > end_scr) {
			
			
			cnt = count_lines(end_scr, beg_cur);
			if (cnt > ((G.rows               ) - 1) / 2)
				goto sc1;	
			for (ro = 0; ro < cnt - 1; ro++) {
				
				(G.screenbegin        ) = next_line((G.screenbegin        ));
				
				end_scr = next_line(end_scr);
				end_scr = end_line(end_scr);
			}
		}
	}
	
	tp = (G.screenbegin        );
	for (ro = 0; ro < (G.rows               ) - 1; ro++) {	
		if (tp == beg_cur)
			break;
		tp = next_line(tp);
	}

	
	co = 0;
	while (tp < d) { 
		if (*tp == '\n') 
			break;
		if (*tp == '\t') {
			
			if (d == tp && ((G.cmd_mode           ) & CMODES) != CMODE_COMMAND) {
				break;
			}
			co = next_tabstop(co);
		} else if ((unsigned char)*tp < ' ' || *tp == 0x7f) {
			co++; 
		}
		co++;
		tp++;
	}

	
	
	
	
	
	
	
	
	
	
	
	

	if (co < 0 + (G.offset             )) {
		(G.offset             ) = co;
	}
	if (co >= (G.columns            ) + (G.offset             )) {
		(G.offset             ) = co - (G.columns            ) + 1;
	}
	
	
	if (d == beg_cur && *d == '\t') {
		(G.offset             ) = 0;
	}
	co -= (G.offset             );

	*row = ro;
	*col = co;
}


static char *begin_line(char *p) 
{
	if (p > (G.text          )) {
		p = memrchr((G.text          ), '\n', p - (G.text          ));
		if (!p)
			return (G.text          );
		return p + 1;
	}
	return p;
}

static char *end_line(char *p) 
{
	if (p < (G.end           ) - 1) {
		p = memchr(p, '\n', (G.end           ) - p - 1);
		if (!p)
			return (G.end           ) - 1;
	}
	return p;
}

static char *dollar_line(char *p) 
{
	p = end_line(p);
	
	if (*p == '\n' && (p - begin_line(p)) > 0)
		p--;
	return p;
}

static char *prev_line(char *p) 
{
	p = begin_line(p);	
	if (p > (G.text          ) && p[-1] == '\n')
		p--;			
	p = begin_line(p);	
	return p;
}

static char *next_line(char *p) 
{
	p = end_line(p);
	if (p < (G.end           ) - 1 && *p == '\n')
		p++;			
	return p;
}


static char *end_screen(void)
{
	char *q;
	int cnt;

	
	q = (G.screenbegin        );
	for (cnt = 0; cnt < (G.rows               ) - 2; cnt++)
		q = next_line(q);
	q = end_line(q);
	return q;
}


static int count_lines(char *start, char *stop)
{
	char *q;
	int cnt;

	if (stop < start) { 
		q = start;
		start = stop;
		stop = q;
	}
	cnt = 0;
	stop = end_line(stop);
	while (start <= stop && start <= (G.end           ) - 1) {
		start = end_line(start);
		if (*start == '\n')
			cnt++;
		start++;
	}
	return cnt;
}

static char *find_line(int li)	
{
	char *q;

	for (q = (G.text          ); li > 1; li--) {
		q = next_line(q);
	}
	return q;
}


static void dot_left(void)
{
	if ((G.dot           ) > (G.text          ) && (G.dot           )[-1] != '\n')
		(G.dot           )--;
}

static void dot_right(void)
{
	if ((G.dot           ) < (G.end           ) - 1 && *(G.dot           ) != '\n')
		(G.dot           )++;
}

static void dot_begin(void)
{
	(G.dot           ) = begin_line((G.dot           ));	
}

static void dot_end(void)
{
	(G.dot           ) = end_line((G.dot           ));	
}

static char *move_to_col(char *p, int l)
{
	int co;

	p = begin_line(p);
	co = 0;
	while (co < l && p < (G.end           )) {
		if (*p == '\n') 
			break;
		if (*p == '\t') {
			co = next_tabstop(co);
		} else if (*p < ' ' || *p == 127) {
			co++; 
		}
		co++;
		p++;
	}
	return p;
}

static void dot_next(void)
{
	(G.dot           ) = next_line((G.dot           ));
}

static void dot_prev(void)
{
	(G.dot           ) = prev_line((G.dot           ));
}

static void dot_scroll(int cnt, int dir)
{
	char *q;

	for (; cnt > 0; cnt--) {
		if (dir < 0) {
			
			
			(G.screenbegin        ) = prev_line((G.screenbegin        ));
		} else {
			
			
			(G.screenbegin        ) = next_line((G.screenbegin        ));
		}
	}
	
	if ((G.dot           ) < (G.screenbegin        ))
		(G.dot           ) = (G.screenbegin        );
	q = end_screen();	
	if ((G.dot           ) > q)
		(G.dot           ) = begin_line(q);	
	dot_skip_over_ws();
}

static void dot_skip_over_ws(void)
{
	
	while (isspace(*(G.dot           )) && *(G.dot           ) != '\n' && (G.dot           ) < (G.end           ) - 1)
		(G.dot           )++;
}

static void dot_delete(void)	
{
	text_hole_delete((G.dot           ), (G.dot           ));
}

static char *bound_dot(char *p) 
{
	if (p >= (G.end           ) && (G.end           ) > (G.text          )) {
		p = (G.end           ) - 1;
		Indicate_Error();
	}
	if (p < (G.text          )) {
		p = (G.text          );
		Indicate_Error();
	}
	return p;
}





        
#1987 /home/user/UCSD-C/ports/vi/vi.c

static char *new_screen(int ro, int co)
{
	free((G.screen             ));
	(G.screensize         ) = ro * co + 8;
	(G.screen             ) = xmalloc((G.screensize         ));
	screen_erase();
	return (G.screen             );
}


static int mycmp(const char *s1, const char *s2, int len)
{
	int i;

	i = strncmp(s1, s2, len);
	if (1 && ((G.vi_setops          ) & 4)) {
		i = strncasecmp(s1, s2, len);
	}
	return i;
}


static char *char_search(char *p, const char *pat, int dir, int range)
{

	char *start, *stop;
	int len;

	len = strlen(pat);
	if (dir == FORWARD) {
		stop = (G.end           ) - 1;	
		if (range == LIMITED)
			stop = next_line(p);	
		for (start = p; start < stop; start++) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	} else if (dir == BACK) {
		stop = (G.text          );	
		if (range == LIMITED)
			stop = prev_line(p);	
		for (start = p - len; start >= stop; start--) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	}
	
	return ((void *)0);






























































}


static char *char_insert(char *p, char c) 
{
	if (c == 22) {		
		p = stupid_insert(p, '^');	
		p--;			
		refresh();		
		c = get_one_char();
		*p = c;
		p++;
		(G.file_modified      )++;
	} else if (c == 27) {	
		(G.cmd_mode           ) = CMODE_COMMAND;
		(G.cmdcnt             ) = 0;
		end_cmd_q();	
		if ((p[-1] != '\n') && ((G.dot           ) > (G.text          ))) {
			p--;
		}
	} else if (c == (G.erase_char         ) || c == 8 || c == 127) { 
		
		if ((p[-1] != '\n') && ((G.dot           )>(G.text          ))) {
			p--;
			p = text_hole_delete(p, p);	
		}
	} else {
		
		char *sp;		

		if (c == 13)
			c = '\n';	
		sp = p;			
		p = stupid_insert(p, c);	

		if (((G.vi_setops          ) & 2 ) && strchr(")]}", *sp) != ((void *)0)) {
			showmatching(sp);
		}
		if (((G.vi_setops          ) & 1) && c == '\n') {	
			char *q;

			q = prev_line(p);	
			for (; ((*q) == ' ' || (*q) == '\t'); q++) {
				p = stupid_insert(p, *q);	
			}
		}

	}
	return p;
}

static char *stupid_insert(char *p, char c) 
{
	p = text_hole_make(p, 1);
	*p = c;
	
	return p + 1;
}

static int find_range(char **start, char **stop, char c)
{
	char *save_dot, *p, *q, *t;
	int cnt, multiline = 0;

	save_dot = (G.dot           );
	p = q = (G.dot           );

	if (strchr("cdy><", c)) {
		
		p = q = begin_line(p);
		for (cnt = 1; cnt < (G.cmdcnt             ); cnt++) {
			q = next_line(q);
		}
		q = end_line(q);
	} else if (strchr("^%$0bBeEfth\b\177", c)) {
		
		do_cmd(c);		
		q = (G.dot           );
	} else if (strchr("wW", c)) {
		do_cmd(c);		
		
		
		
		if ((G.dot           ) > (G.text          ) && ((isspace((G.dot           )[-1]) && !isspace((G.dot           )[0])) 				|| (ispunct((G.dot           )[-1]) && !ispunct((G.dot           )[0])) 				|| (isalnum((G.dot           )[-1]) && !isalnum((G.dot           )[0]))))
#2186 /home/user/UCSD-C/ports/vi/vi.c
			(G.dot           )--;		
		if ((G.dot           ) > (G.text          ) && *(G.dot           ) == '\n')
			(G.dot           )--;		
		q = (G.dot           );
	} else if (strchr("H-k{", c)) {
		
		q = end_line((G.dot           ));	
		do_cmd(c);		
		dot_begin();
		p = (G.dot           );
	} else if (strchr("L+j}\r\n", c)) {
		
		p = begin_line((G.dot           ));
		do_cmd(c);		
		dot_end();		
		q = (G.dot           );
	} else {
	    
	    
	    
	    
	    
	}
	if (q < p) {
		t = q;
		q = p;
		p = t;
	}

	
	if (q > p && strchr("^0bBh\b\177", c)) q--;

	multiline = 0;
	for (t = p; t <= q; t++) {
		if (*t == '\n') {
			multiline = 1;
			break;
		}
	}

	*start = p;
	*stop = q;
	(G.dot           ) = save_dot;
	return multiline;
}

static int st_test(char *p, int type, int dir, char *tested)
{
	char c, c0, ci;
	int test, inc;

	inc = dir;
	c = c0 = p[0];
	ci = p[inc];
	test = 0;

	if (type == S_BEFORE_WS) {
		c = ci;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_TO_WS) {
		c = c0;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_OVER_WS) {
		c = c0;
		test = ((isspace(c)));
	}
	if (type == S_END_PUNCT) {
		c = ci;
		test = ((ispunct(c)));
	}
	if (type == S_END_ALNUM) {
		c = ci;
		test = ((isalnum(c)) || c == '_');
	}
	*tested = c;
	return test;
}

static char *skip_thing(char *p, int linecnt, int dir, int type)
{
	char c;

	while (st_test(p, type, dir, &c)) {
		
		if (c == '\n' && --linecnt < 1)
			break;
		if (dir >= 0 && p >= (G.end           ) - 1)
			break;
		if (dir < 0 && p <= (G.text          ))
			break;
		p += dir;		
	}
	return p;
}


static char *find_pair(char *p, const char c)
{
	char match, *q;
	int dir, level;

	match = ')';
	level = 1;
	dir = 1;			
	switch (c) {
	case '(': match = ')'; break;
	case '[': match = ']'; break;
	case '{': match = '}'; break;
	case ')': match = '('; dir = -1; break;
	case ']': match = '['; dir = -1; break;
	case '}': match = '{'; dir = -1; break;
	}
	for (q = p + dir; (G.text          ) <= q && q < (G.end           ); q += dir) {
		
		if (*q == c)
			level++;	
		if (*q == match)
			level--;	
		if (level == 0)
			break;		
	}
	if (level != 0)
		q = ((void *)0);		
	return q;
}


static char *text_hole_make(char *p, int size)	
{
	if (size <= 0)
		return p;
	(G.end           ) += size;		
	if ((G.end           ) >= ((G.text          ) + (G.text_size     ))) {
		char *new_text;
		(G.text_size     ) += (G.end           ) - ((G.text          ) + (G.text_size     )) + 1024;
		new_text = xrealloc((G.text          ), (G.text_size     ));
		(G.screenbegin        ) = new_text + ((G.screenbegin        ) - (G.text          ));
		(G.dot           )         = new_text + ((G.dot           )         - (G.text          ));
		(G.end           )         = new_text + ((G.end           )         - (G.text          ));
		p           = new_text + (p           - (G.text          ));

		{
			int k;
			for (k = 0; k < 28; k++)
				if ((G.mark          )[k])
					(G.mark          )[k] = new_text + ((G.mark          )[k] - (G.text          ));
			if ((G.context_start ))
				(G.context_start ) = new_text + ((G.context_start ) - (G.text          ));
			if ((G.context_end   ))
				(G.context_end   ) = new_text + ((G.context_end   ) - (G.text          ));
			(G.edit_file__cur_line) = ((void *)0);
		}

		(G.text          ) = new_text;
	}
	memmove(p + size, p, (G.end           ) - size - p);
	memset(p, ' ', size);	
	(G.file_modified      )++;
	return p;
}


static char *text_hole_delete(char *p, char *q) 
{
	char *src, *dest;
	int cnt, hole_size;

	
	
	src = q + 1;
	dest = p;
	if (q < p) {		
		src = p + 1;
		dest = q;
	}
	hole_size = q - p + 1;
	cnt = (G.end           ) - src;
	if (src < (G.text          ) || src > (G.end           ))
		goto thd0;
	if (dest < (G.text          ) || dest >= (G.end           ))
		goto thd0;
	if (src >= (G.end           ))
		goto thd_atend;	
	memmove(dest, src, cnt);
 thd_atend:
	(G.end           ) = (G.end           ) - hole_size;	
	if (dest >= (G.end           ))
		dest = (G.end           ) - 1;	
	if ((G.end           ) <= (G.text          ))
		dest = (G.end           ) = (G.text          );	
	(G.file_modified      )++;
 thd0:
	return dest;
}




static char *yank_delete(char *start, char *stop, int dist, int yf)
{
	char *p;

	
	if (start > stop) {
		
		p = start;
		start = stop;
		stop = p;
	}
	if (dist <= 0) {
		
		p = start;
		if (*p == '\n')
			return p;
		
		for (; p + 1 <= stop; p++) {
			if (p[1] == '\n') {
				stop = p;	
				break;
			}
		}
	}
	p = start;

	text_yank(start, stop, (G.YDreg         ));

	if (yf == YANKDEL) {
		p = text_hole_delete(start, stop);
	}					
	return p;
}








































static void start_new_cmd_q(char c)
{
	
	
	if ((G.cmdcnt             ) > 0)
		(G.lmc_len            ) = sprintf((G.last_modifying_cmd ), "%d%c", (G.cmdcnt             ), c);
	else { 
		(G.last_modifying_cmd )[0] = c;
		(G.lmc_len            ) = 1;
	}
	(G.adding2q           ) = 1;
}

static void end_cmd_q(void)
{

	(G.YDreg         ) = 26;			

	(G.adding2q           ) = 0;
}



#2483 /home/user/UCSD-C/ports/vi/vi.c
static char *string_insert(char *p, char *s) 
{
	int cnt, i;

	i = strlen(s);
	text_hole_make(p, i);
	strncpy(p, s, i);
	for (cnt = 0; *s != '\0'; s++) {
		if (*s == '\n')
			cnt++;
	}

	status_line("Put %d lines (%d chars) from [%c]", cnt, i, what_reg());

	return p;
}



static char *text_yank(char *p, char *q, int dest)	
{
	char *t;
	int cnt;

	if (q < p) {		
		t = q;
		q = p;
		p = t;
	}
	cnt = q - p + 1;
	t = (G.reg           )[dest];
	free(t);		
	t = xmalloc(cnt + 1);	
	memset(t, '\0', cnt + 1);	
	strncpy(t, p, cnt);	
	(G.reg           )[dest] = t;
	return p;
}

static char what_reg(void)
{
	char c;

	c = 'D';			
	if (0 <= (G.YDreg         ) && (G.YDreg         ) <= 25)
		c = 'a' + (char) (G.YDreg         );
	if ((G.YDreg         ) == 26)
		c = 'D';
	if ((G.YDreg         ) == 27)
		c = 'U';
	return c;
}

static void check_context(char cmd)
{
	
	

	if ((G.dot           ) < (G.context_start ) || (G.dot           ) > (G.context_end   )) {
		if (strchr((G.modifying_cmds     ), cmd) != ((void *)0)) {
			
			(G.mark          )[27] = (G.mark          )[26];	
			(G.mark          )[26] = (G.dot           );	
			(G.context_start ) = prev_line(prev_line((G.dot           )));
			(G.context_end   ) = next_line(next_line((G.dot           )));
			
		}
	}
}

static char *swap_context(char *p) 
{
	char *tmp;

	
	
	
	if ((G.text          ) <= (G.mark          )[27] && (G.mark          )[27] <= (G.end           ) - 1) {
		tmp = (G.mark          )[27];
		(G.mark          )[27] = (G.mark          )[26];
		(G.mark          )[26] = tmp;
		p = (G.mark          )[26];	
		(G.context_start ) = prev_line(prev_line(prev_line(p)));
		(G.context_end   ) = next_line(next_line(next_line(p)));
	}
	return p;
}





































































































































































































































static char get_one_char(void)
{
	char c;


	if (!(G.adding2q           )) {
		
		
		if ((G.ioq                ) == 0) {
			
			c = readit();	
		} else {
			
			c = *(G.ioq                )++;
			if (c == '\0') {
				
				free((G.ioq_start          ));
				(G.ioq_start          ) = (G.ioq                ) = 0;
				c = readit();	
			}
		}
	} else {
		
		c = readit();	
		if ((G.lmc_len            ) >= MAX_INPUT_LEN - 1) {
			status_line_bold("last_modifying_cmd overrun");
		} else {
			
			(G.last_modifying_cmd )[(G.lmc_len            )++] = c;
		}
	}



	return c;
}


static char *get_input_line(const char *prompt)
{
	


	char c;
	int i;

	*(G.displayed_buffer   ) = 0;	
	(G.cmd_mode           ) |= CMODE_LINE_INPUT;
	strcpy((G.status_buffer      ), prompt);
	place_cursor((G.rows               ) - 1, 0);	
	clear_to_eol();		
	write1(prompt);      

	i = strlen((G.status_buffer      ));
	while (i < MAX_INPUT_LEN) {
		c = get_one_char();
		if (c == '\n' || c == '\r' || c == 27)
			break;		
		if (c == (G.erase_char         ) || c == 8 || c == 127) {
			
			(G.status_buffer      )[--i] = '\0';
			write1("\b \b"); 
			if (i <= 0) 
				break;
		} else {
			(G.status_buffer      )[i] = c;
			(G.status_buffer      )[++i] = '\0';
			bb_putchar(c);
		}
	}
	(G.cmd_mode           ) &= ~CMODE_LINE_INPUT;
	return strcpy((G.get_input_line__buf), (G.status_buffer      ));

}






































































































































































































static void standout_start(void) 
{
	write1(SOs);     
}


static void standout_end(void) 
{
	write1(SOn);     
}


static void flash(int h)
{
	standout_start();	
	redraw();
	awaitInput(h);
	standout_end();		
	redraw();
}

static void Indicate_Error(void)
{
	if (!((G.vi_setops          ) & 8)) {
		write1(bell);   
	} else {
		flash(10);
	}
}



static void screen_erase(void)
{
	memset((G.screen             ), ' ', (G.screensize         ));	
}

static const char *scompare(const char *s, const char *ref)

{
	while(*s && *s == *ref)
		s++, ref++;
	return *s == *ref ? ((void *)0) : s;
}


static void show_status_line(void)
{
	
	
	const char *buffer = (G.status_buffer      );
	if (!*buffer)
	  format_edit_status("%s: %s%s%s line %d/%d %d%%");
	const char *changed = scompare(buffer, (G.displayed_buffer   ));
	if (changed) {
		size_t unchanged = changed - buffer;
    	strcpy((G.displayed_buffer   )+unchanged, changed);
    	
		size_t escapes = *buffer == *SOs ? 2*(4) : 0;
		place_cursor((G.rows               ) - 1, escapes ? unchanged - (4) : unchanged);
#3132 /home/user/UCSD-C/ports/vi/vi.c
		clear_to_eol(); 
		if (unchanged && escapes)   
			fwrite(buffer, (4), 1, (&__files[1]));
		size_t len = unchanged + strlen(buffer=changed);
		if (len - escapes > (G.columns            )) {
			const char *limit = (G.status_buffer      ) + (G.columns            );
			if (escapes)
				limit += (4);
			fwrite(buffer, limit-buffer, 1, (&__files[1]));
			buffer = (G.status_buffer      ) + len;
			if (escapes && len > 2*(4)) 
				buffer-=(4);
		}
		write1(buffer);  
		if (!((G.cmd_mode           ) & CMODE_LINE_INPUT))
			place_cursor((G.crow               ), (G.ccol               )); 
	}else if ((G.cmd_mode           ) & CMODE_LINE_INPUT)  
		place_cursor((G.rows               )-1, strlen(buffer));  
	else  
		place_cursor((G.crow               ), (G.ccol               ));
}



static void status_line_bold(const char *format, ...)
{
	va_list args;

	((args) = __va_start());
	strcpy((G.status_buffer      ), SOs);	
	vsprintf((G.status_buffer      ) + sizeof(SOs)-1, format, args);
	strcat((G.status_buffer      ), SOn);	
	((void)0);
}


static void status_line(const char *format, ...)
{
	va_list args;

	((args) = __va_start());
	vsprintf((G.status_buffer      ), format, args);
	((void)0);
}


static void print_literal(char *buf, const char *s)
{
	unsigned char c;
	char b[2];

	b[1] = '\0';
	buf[0] = '\0';
	if (!s[0])
		s = "(NULL)";
	for (; *s; s++) {
		int c_is_no_print;

		c = *s;
		c_is_no_print = (c & 0x80) && !((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f);
		if (c_is_no_print) {
			strcat(buf, SOn);
			c = '.';
		}
		if (c < ' ' || c == 127) {
			strcat(buf, "^");
			if (c == 127)
				c = '?';
			else
				c += '@';
		}
		b[0] = c;
		strcat(buf, b);
		if (c_is_no_print)
			strcat(buf, SOs);
		if (*s == '\n')
			strcat(buf, "$");
		if (strlen(buf) > MAX_INPUT_LEN - 10) 
			break;
	}
}

static void not_implemented(const char *s)
{
	char buf[MAX_INPUT_LEN];

	print_literal(buf, s);
	status_line_bold("\'%s\' is not implemented", buf);
}


static int format_edit_status(const char *fmt)
{


	int cur, percent, ret, trunc_at;

	
	
	
	

	
	
	
	cur = count_lines((G.text          ), (G.dot           ));

	
	
	if ((G.file_modified      ) != (G.last_file_modified )) {
		(G.format_edit_status__tot) = cur + count_lines((G.dot           ), (G.end           ) - 1) - 1;
		(G.last_file_modified ) = (G.file_modified      );
	}

	
	
	
	if ((G.format_edit_status__tot) > 0) {
		percent = (100 * cur) / (G.format_edit_status__tot);
	} else {
		cur = (G.format_edit_status__tot) = 0;
		percent = 100;
	}

	trunc_at = (G.columns            ) < 200-1 ?
		(G.columns            ) : 200-1;

	ret = snprintf((G.status_buffer      ), trunc_at+1, fmt, 		cmd_mode_indicator[(G.cmd_mode           ) & CMODES], 		((G.current_filename   ) != ((void *)0) ? (G.current_filename   ) : "No file"), 		((G.readonly_mode      ) ? " [Readonly]" : ""), 		((G.file_modified      ) ? " [Modified]" : ""), 		cur, (G.format_edit_status__tot), percent, (G.offset             )+(G.ccol               )+1, (G.cmdcnt             ));
#3267 /home/user/UCSD-C/ports/vi/vi.c

	if (ret >= 0 && ret < trunc_at)
		return ret;   

	return trunc_at;   

}


static void redraw(void)
{
	clear_screen();		
	screen_erase();
	refresh();
}


static char* format_line(char *src  )
{
	unsigned char c;
	int co;
	int ofs = (G.offset             );
	char *dest = (G.scr_out_buf   ); 

	c = '~'; 
	co = 0;
	while (co < (G.columns            ) + (G.tabstop            )) {
		
		if (src < (G.end           )) {
			c = *src++;
			if (c == '\n')
				break;
			if ((c & 0x80) && !((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f)) {
				c = '.';
			}
			if (c < ' ' || c == 0x7f) {
				if (c == '\t') {
					c = ' ';
					
					while ((co % (G.tabstop            )) != ((G.tabstop            ) - 1)) {
						dest[co++] = c;
					}
				} else {
					dest[co++] = '^';
					if (c == 0x7f)
						c = '?';
					else
						c += '@'; 
				}
			}
		}
		dest[co++] = c;
		
		
		if (ofs >= (G.tabstop            ) && co >= (G.tabstop            )) {
			memmove(dest, dest + (G.tabstop            ), co);
			co -= (G.tabstop            );
			ofs -= (G.tabstop            );
		}
		if (src >= (G.end           ))
			break;
	}
	
	if (co < ofs)
		ofs = co;
	
	co -= ofs;
	dest += ofs;
	
	if (co < (G.columns            ))
		memset(&dest[co], ' ', (G.columns            ) - co);
	return dest;
}






static void refresh(void)
{


	int li, changed;
	char *tp, *sp;		

	
	
	
	if ((G.chars_to_parse     ) || awaitInput(0))
		return;

	sync_cursor((G.dot           ), &(G.crow               ), &(G.ccol               ));	
	tp = (G.screenbegin        );	

	
	for (li = 0; li < (G.rows               ) - 1 && !awaitInput(0); li++) {
		int cs, ce;				
		char *out_buf;
		
		out_buf = format_line(tp  );

		
		if (tp < (G.end           )) {
			char *t = memchr(tp, '\n', (G.end           ) - tp);
			if (!t) t = (G.end           ) - 1;
			tp = t + 1;
		}

		
		changed = 0;	
		cs = 0;
		ce = (G.columns            ) - 1;
		sp = &(G.screen             )[li * (G.columns            )];	
		
		
		for (; cs <= ce; cs++) {
			if (out_buf[cs] != sp[cs]) {
				changed = 1;	
				break;
			}
		}

		
		for (; ce >= cs; ce--) {
			if (out_buf[ce] != sp[ce]) {
				changed = 1;	
				break;
			}
		}
		

		
		if ((G.offset             ) != (G.refresh__old_offset)) {
			changed = 1;
		}

		
		if (cs < 0) cs = 0;
		if (ce > (G.columns            ) - 1) ce = (G.columns            ) - 1;
		if (cs > ce) { cs = 0; ce = (G.columns            ) - 1; }
		
		if (changed) {
			
			memcpy(sp+cs, out_buf+cs, ce-cs+1);

			
			
			
			
			
			
				place_cursor(li, cs);
			

			
			fwrite(&sp[cs], ce - cs + 1, 1, (&__files[1]));
		}
	}

	place_cursor((G.crow               ), (G.ccol               ));

	(G.refresh__old_offset) = (G.offset             );

	show_status_line();
}

























static int do_cmd2(char *cp)
{

	const char *msg;
	char c1, *p, *q, *save_dot;
	char buf[12];
	int dir, cnt, i, j;

	switch ((*cp)) {
	default:			
		buf[0] = (*cp);
		buf[1] = '\0';
		if ((*cp) < ' ') {
			buf[0] = '^';
			buf[1] = (*cp) + '@';
			buf[2] = '\0';
		}
		not_implemented(buf);
		end_cmd_q();	
		break;
	case '0':			
	case '1':			
	case '2':			
	case '3':			
	case '4':			
	case '5':			
	case '6':			
	case '7':			
	case '8':			
	case '9':			
		if ((*cp) == '0' && (G.cmdcnt             ) < 1) {
			dot_begin();	
		} else {
			if ((G.cmdcnt             ) >= 32767/10) {
				(G.cmdcnt             ) = 0;
				status_line_bold("Repeat Count OVERFLOW");
				return 4;
			}
			(G.cmdcnt             ) = (G.cmdcnt             ) * 10 + ((*cp) - '0');	
			if ((G.cmdcnt             )!=1)
			  format_edit_status("%s: %s%s%s line %d/%d %d%%" " col %d {%d times}");
		}
		break;
	case ':':			
		p = get_input_line(":");	

		colon(p);		









































		break;
	case '<':			
	case '>':			
		cnt = count_lines((G.text          ), (G.dot           ));	
		c1 = get_one_char();	
		find_range(&p, &q, c1);
		yank_delete(p, q, 1, YANKONLY);	
		p = begin_line(p);
		q = end_line(q);
		i = count_lines(p, q);	
		for ( ; i > 0; i--, p = next_line(p)) {
			if ((*cp) == '<') {
				
				if (*p == '\t') {
					
					text_hole_delete(p, p);
				} else if (*p == ' ') {
					
					for (j = 0; *p == ' ' && j < (G.tabstop            ); j++) {
						text_hole_delete(p, p);
					}
				}
			} else if ((*cp) == '>') {
				
				char_insert(p, '\t');
			}
		}
		(G.dot           ) = find_line(cnt);	
		dot_skip_over_ws();
		end_cmd_q();	
		break;
	case 'A':			
		dot_end();		
		
	case 'a':			
		if (*(G.dot           ) != '\n')
			(G.dot           )++;
		goto dc_i;
		break;
	case 'B':			
	case 'E':			
	case 'W':			
		dir = FORWARD;
		if ((*cp) == 'B')
			dir = BACK;
		if ((*cp) == 'W' || isspace((G.dot           )[dir])) {
			(G.dot           ) = skip_thing((G.dot           ), 1, dir, S_TO_WS);
			(G.dot           ) = skip_thing((G.dot           ), 2, dir, S_OVER_WS);
		}
		if ((*cp) != 'W')
			(G.dot           ) = skip_thing((G.dot           ), 1, dir, S_BEFORE_WS);
		return 1;
	case 'C':			
	case 'D':			
		save_dot = (G.dot           );
		(G.dot           ) = dollar_line((G.dot           ));	
		
		(G.dot           ) = yank_delete(save_dot, (G.dot           ), 0, YANKDEL);	
		if ((*cp) == 'C')
			goto dc_i;	

		if ((*cp) == 'D')
			end_cmd_q();	

		break;
	case 'g':                       
					
		c1 = get_one_char();
		if (c1 != 'g') {
			buf[0] = 'g';
			buf[1] = c1;
			buf[2] = '\0';
			not_implemented(buf);
			break;
		}
		if ((G.cmdcnt             ) == 0)
			(G.cmdcnt             ) = 1;
		 
	case 'G':		
		(G.dot           ) = (G.end           ) - 1;				
		if ((G.cmdcnt             ) > 0) {
			(G.dot           ) = find_line((G.cmdcnt             ));	
		}
		dot_skip_over_ws();
		break;
	case 'H':			
		(G.dot           ) = (G.screenbegin        );
		if ((G.cmdcnt             ) > ((G.rows               ) - 1)) {
			(G.cmdcnt             ) = ((G.rows               ) - 1);
		}
		if ((G.cmdcnt             )-- > 1) {
			do_cmd('+');
		}				
		dot_skip_over_ws();
		break;
	case 'I':			
		dot_begin();	
		dot_skip_over_ws();
		
	case 'i':			
	case (char)134:	
dc_i:
		(G.cmd_mode           ) = CMODE_INSERT;	
		break;
	case 'J':			
		dot_end();		
		if ((G.dot           ) < (G.end           ) - 1) {	
			*(G.dot           )++ = ' ';	
			(G.file_modified      )++;
			while (((*(G.dot           )) == ' ' || (*(G.dot           )) == '\t')) {	
				dot_delete();
			}
		}
		end_cmd_q();	
		if ((G.cmdcnt             )-- > 2)
			return 2;
		break;
	case 'L':			
		(G.dot           ) = end_screen();
		if ((G.cmdcnt             ) > ((G.rows               ) - 1)) {
			(G.cmdcnt             ) = ((G.rows               ) - 1);
		}
		if ((G.cmdcnt             )-- > 1) {
			do_cmd('-');
		}				
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'M':			
		(G.dot           ) = (G.screenbegin        );
		for (cnt = 0; cnt < ((G.rows               )-1) / 2; cnt++)
			(G.dot           ) = next_line((G.dot           ));
		break;
	case 'O':			
		
		p = begin_line((G.dot           ));
		if (p[-1] == '\n') {
			dot_prev();
	case 'o':			
			dot_end();
			(G.dot           ) = char_insert((G.dot           ), '\n');
		} else {
			dot_begin();	
			(G.dot           ) = char_insert((G.dot           ), '\n');	
			dot_prev();	
		}
		goto dc_i;
		break;
	case 'R':			
dc5:
		(G.cmd_mode           ) = CMODE_REPLACE;
		break;
	case (char)135:
		(*cp) = 'x';
		
	case 'X':			
	case 'x':			
	case 's':			
		dir = 0;
		if ((*cp) == 'X')
			dir = -1;
		if ((G.dot           )[dir] != '\n') {
			if ((*cp) == 'X')
				(G.dot           )--;	
			(G.dot           ) = yank_delete((G.dot           ), (G.dot           ), 0, YANKDEL);	
		}
		if ((*cp) == 's')
			goto dc_i;	
		end_cmd_q();	
		return 1;
	case 'Z':			
		
		c1 = get_one_char();
		if (c1 != 'Z') {
			Indicate_Error();
			break;
		}
		if ((G.file_modified      )) {
			if (1 && (G.readonly_mode      )) {
				status_line_bold("\"%s\" File is read only", (G.current_filename   ));
				break;
			}
			cnt = file_write((G.current_filename   ), (G.text          ), (G.end           ) - 1);
			if (cnt < 0) {
				if (cnt == -1)
					status_line_bold("Write error: %s", strerror((((struct syscomrec *)__osvar(1))->iorslt)));
			} else if (cnt == ((G.end           ) - 1 - (G.text          ) + 1)) {
				(G.editing            ) = 0;
			}
		} else {
			(G.editing            ) = 0;
		}
		break;
	case '^':			
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'b':			
	case 'e':			
		dir = FORWARD;
		if ((*cp) == 'b')
			dir = BACK;
		if (((G.dot           ) + dir) < (G.text          ) || ((G.dot           ) + dir) > (G.end           ) - 1)
			break;
		(G.dot           ) += dir;
		if (isspace(*(G.dot           ))) {
			(G.dot           ) = skip_thing((G.dot           ), ((*cp) == 'e') ? 2 : 1, dir, S_OVER_WS);
		}
		if (isalnum(*(G.dot           )) || *(G.dot           ) == '_') {
			(G.dot           ) = skip_thing((G.dot           ), 1, dir, S_END_ALNUM);
		} else if (ispunct(*(G.dot           ))) {
			(G.dot           ) = skip_thing((G.dot           ), 1, dir, S_END_PUNCT);
		}
		return 1;
	case 'c':			
	case 'd':			

	case 'y':			
	case 'Y':			

		{
		int yf, ml, whole = 0;
		yf = YANKDEL;	

		if ((*cp) == 'y' || (*cp) == 'Y')
			yf = YANKONLY;

		c1 = 'y';
		if ((*cp) != 'Y')
			c1 = get_one_char();	
		
		ml = find_range(&p, &q, c1);
		if (c1 == 27) {	
			(*cp) = c1 = 27;	
		} else if (strchr("wW", c1)) {
			if ((*cp) == 'c') {
				
				while (((*q) == ' ' || (*q) == '\t')) {
					if (q <= (G.text          ) || q[-1] == '\n')
						break;
					q--;
				}
			}
			(G.dot           ) = yank_delete(p, q, ml, yf);	
		} else if (strchr("^0bBeEft%$ lh\b\177", c1)) {
			
			(G.dot           ) = yank_delete(p, q, ml, yf);	
		} else if (strchr("cdykjHL+-{}\r\n", c1)) {
			
			(G.dot           ) = yank_delete(p, q, ml, yf);	
			whole = 1;
		} else {
			
			(*cp) = c1 = 27;	
			ml = 0;
			Indicate_Error();
		}
		if (ml && whole) {
			if ((*cp) == 'c') {
				(G.dot           ) = char_insert((G.dot           ), '\n');
				
				if (whole && (G.dot           ) != ((G.end           )-1)) {
					dot_prev();
				}
			} else if ((*cp) == 'd') {
				dot_begin();
				dot_skip_over_ws();
			}
		}
		if (c1 != 27) {
			
			if ((*cp) == 'c') {
				strcpy(buf, "Change");
				goto dc_i;	
			}
			if ((*cp) == 'd') {
				strcpy(buf, "Delete");
			}

			if ((*cp) == 'y' || (*cp) == 'Y') {
				strcpy(buf, "Yank");
			}
			p = (G.reg           )[(G.YDreg         )];
			q = p + strlen(p);
			for (cnt = 0; p <= q; p++) {
				if (*p == '\n')
					cnt++;
			}
			status_line("%s %d lines (%d chars) using [%c]", 				buf, cnt, strlen((G.reg           )[(G.YDreg         )]), what_reg());
#3836 /home/user/UCSD-C/ports/vi/vi.c

			end_cmd_q();	
		}
		}
		break;
	case 'k':			
	case (char)128:		
		dot_prev();
		(G.dot           ) = move_to_col((G.dot           ), (G.ccol               ) + (G.offset             ));	
		return 1;
	case 'r':			
		c1 = get_one_char();	
		if (*(G.dot           ) != '\n') {
			*(G.dot           ) = c1;
			(G.file_modified      )++;
		}
		end_cmd_q();	
		break;
	case 't':			
		(G.last_forward_char  ) = get_one_char();
		do_cmd(';');
		if (*(G.dot           ) == (G.last_forward_char  ))
			dot_left();
		(G.last_forward_char  )= 0;
		break;
	case 'w':			
		if (isalnum(*(G.dot           )) || *(G.dot           ) == '_') {	
			(G.dot           ) = skip_thing((G.dot           ), 1, FORWARD, S_END_ALNUM);
		} else if (ispunct(*(G.dot           ))) {	
			(G.dot           ) = skip_thing((G.dot           ), 1, FORWARD, S_END_PUNCT);
		}
		if ((G.dot           ) < (G.end           ) - 1)
			(G.dot           )++;		
		if (isspace(*(G.dot           ))) {
			(G.dot           ) = skip_thing((G.dot           ), 2, FORWARD, S_OVER_WS);
		}
		return 1;
	case 'z':			
		c1 = get_one_char();	
		cnt = 0;
		if (c1 == '.')
			cnt = ((G.rows               ) - 2) / 2;	
		if (c1 == '-')
			cnt = (G.rows               ) - 2;	
		(G.screenbegin        ) = begin_line((G.dot           ));	
		dot_scroll(cnt, -1);
		break;
	case '|':			
		(G.dot           ) = move_to_col((G.dot           ), (G.cmdcnt             ) - 1);	
		break;
	case '~':			
		if (islower(*(G.dot           ))) {
			*(G.dot           ) = toupper(*(G.dot           ));
			(G.file_modified      )++;
		} else if (isupper(*(G.dot           ))) {
			*(G.dot           ) = tolower(*(G.dot           ));
			(G.file_modified      )++;
		}
		dot_right();
		end_cmd_q();	
		return 1;
		
	case (char)132:	
		dot_begin();
		break;
		
	case (char)138:	
	case (char)139:	
	case (char)140:	
	case (char)141:	
	case (char)142:	
	case (char)143:	
	case (char)144:	
	case (char)145:	
	case (char)146:	
	case (char)147:	
	case (char)148:	
	case (char)149:	
		break;
	}
	return 0;

}

static void do_cmd(char c)
{
	const char *msg;
	char c1, *p, *q, *save_dot;
	char buf[12];
	int dir, cnt, i, j;

again:
	 
	switch (c) {
		case (char)128:
		case (char)129:
		case (char)131:
		case (char)130:
		case (char)132:
		case (char)133:
		case (char)136:
		case (char)137:
			goto key_cmd_mode;
	}

	if ((G.cmd_mode           ) == CMODE_REPLACE) {
		
		if (c == (char)134)
		{
			(G.cmd_mode           ) = CMODE_INSERT;
			goto dc1;
		}
		
		if (*(G.dot           ) == '\n') {
			
			(G.cmd_mode           ) = CMODE_INSERT;	
		} else {
			if (1 <= c || ((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f)) {
				if (c != 27)
					(G.dot           ) = yank_delete((G.dot           ), (G.dot           ), 0, YANKDEL);	
				(G.dot           ) = char_insert((G.dot           ), c);	
			}
			goto dc1;
		}
	}
	if ((G.cmd_mode           ) == CMODE_INSERT) {
		
		if (c == (char)134) {
			(G.cmd_mode           ) = CMODE_REPLACE;
			goto dc1;
		}
		
		if (1 <= c || ((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f)) {
			(G.dot           ) = char_insert((G.dot           ), c);
		}
		goto dc1;
	}

 key_cmd_mode:
	switch (c) {
	default:			
		switch (do_cmd2(&c)) {
		case 1:
			goto repeat;
		case 2:
			goto again;
		case 4:
			return;
		}
		break;
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
	case 0x00:			
		break;
	case 2:			
	case (char)136:	
		dot_scroll((G.rows               ) - 2, -1);
		break;
	case 4:			
		dot_scroll(((G.rows               ) - 2) / 2, 1);
		break;
	case 5:			
		dot_scroll(1, 1);
		break;
	case 6:			
	case (char)137:	
		dot_scroll((G.rows               ) - 2, 1);
		break;
	case 7:			
		format_edit_status("%s: %s%s%s line %d/%d %d%%" " col %d");
		break;
	case 'h':			
	case (char)131:	
	case 8:		
	case 0x7f:	
		dot_left();
repeat:
		if ((G.cmdcnt             )-- > 1)
			goto again;
		break;
	case 10:			
	case 'j':			
	case (char)129:	
		dot_next();		
		(G.dot           ) = move_to_col((G.dot           ), (G.ccol               ) + (G.offset             ));	
		goto repeat;
	case 12:			
	case 18:			
		createScreen();
		redraw();
		break;
	case 13:			
	case '+':			
		dot_next();
		dot_skip_over_ws();
		goto repeat;
	case 21:			
		dot_scroll(((G.rows               ) - 2) / 2, -1);
		break;
	case 25:			
		dot_scroll(1, -1);
		break;
	case 27:			
		if ((G.cmd_mode           ) == CMODE_COMMAND)
			Indicate_Error();
		(G.cmd_mode           ) = CMODE_COMMAND;	
		end_cmd_q();
		break;
	case ' ':			
	case 'l':			
	case (char)130:	
		dot_right();
		goto repeat;

	case '"':			
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			(G.YDreg         ) = c1 - 'a';
		} else {
			Indicate_Error();
		}
		break;
	case '\'':			
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			
			q = (G.mark          )[(unsigned char) c1];
			if ((G.text          ) <= q && q < (G.end           )) {
				(G.dot           ) = q;
				dot_begin();	
				dot_skip_over_ws();
			}
		} else if (c1 == '\'') {	
			(G.dot           ) = swap_context((G.dot           ));	
			dot_begin();	
			dot_skip_over_ws();
		} else {
			Indicate_Error();
		}
		break;
	case 'm':			
		
		
		
		
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			
			(G.mark          )[(int) c1] = (G.dot           );
		} else {
			Indicate_Error();
		}
		break;
	case 'P':			
	case 'p':			
		p = (G.reg           )[(G.YDreg         )];
		if (p == 0) {
			status_line_bold("Nothing in register %c", what_reg());
			break;
		}
		
		if (strchr(p, '\n') != ((void *)0)) {
			if (c == 'P') {
				dot_begin();	
			}
			if (c == 'p') {
				
				if (end_line((G.dot           )) == ((G.end           ) - 1)) {
					(G.dot           ) = (G.end           );	
				} else {
					dot_next();	
				}
			}
		} else {
			if (c == 'p')
				dot_right();	
		}
		(G.dot           ) = string_insert((G.dot           ), p);	
		end_cmd_q();	
		break;
	case 'U':			
		if ((G.reg           )[(G.Ureg          )] != 0) {
			p = begin_line((G.dot           ));
			q = end_line((G.dot           ));
			p = text_hole_delete(p, q);	
			p = string_insert(p, (G.reg           )[(G.Ureg          )]);	
			(G.dot           ) = p;
			dot_skip_over_ws();
		}
		break;

	case '$':			
	case (char)133:		
		(G.dot           ) = end_line((G.dot           ));
		goto repeat;
	case '%':			
		for (q = (G.dot           ); q < (G.end           ) && *q != '\n'; q++) {
			if (strchr("()[]{}", *q) != ((void *)0)) {
				
				p = find_pair(q, *q);
				if (p == ((void *)0)) {
					Indicate_Error();
				} else {
					(G.dot           ) = p;
				}
				break;
			}
		}
		if (*q == '\n')
			Indicate_Error();
		break;
	case 'f':			
		(G.last_forward_char  ) = get_one_char();	
		
		
		
		
	case ';':			
		if ((G.last_forward_char  ) == 0)
			break;
		q = (G.dot           ) + 1;
		while (q < (G.end           ) - 1 && *q != '\n' && *q != (G.last_forward_char  )) {
			q++;
		}
		if (*q == (G.last_forward_char  ))
			(G.dot           ) = q;
		c = ';'; goto repeat;
	case ',':           
		if ((G.last_forward_char  ) == 0)
			break;
		q = (G.dot           ) - 1;
		while (q >= (G.text          ) && *q != '\n' && *q != (G.last_forward_char  )) {
			q--;
		}
		if (q >= (G.text          ) && *q == (G.last_forward_char  ))
			(G.dot           ) = q;
		c = ','; goto repeat;

	case '-':			
		dot_prev();
		dot_skip_over_ws();
		goto repeat;

	case '.':			
		
		
		if ((G.lmc_len            ) > 0) {
			(G.last_modifying_cmd )[(G.lmc_len            )] = 0;
			(G.ioq                ) = (G.ioq_start          ) = xstrdup((G.last_modifying_cmd ));
		}
		break;


	case '?':			
	case '/':			
		buf[0] = c;
		buf[1] = '\0';
		q = get_input_line(buf);	
		if (!*q)
			break;	
		if (q[1]) { 
			free((G.last_search_pattern));
			(G.last_search_pattern) = xstrdup(q);
		    goto findNormal;	
		}  
		if (c == '/')
			goto findNormal;
		c = 'N';
	case 'N':		 
		dir = BACK;  
		goto findPattern;

findNormal:
		c = 'n';
	case 'n':		
		
		
		dir = FORWARD; 
findPattern:
		if (!(G.last_search_pattern)) {
			msg = "No previous regular expression";
			goto dc2;
		}  
		if (*(G.last_search_pattern) == '?')
			dir = -dir;  
		p = (G.dot           ) + dir;
		q = char_search(p, (G.last_search_pattern) + 1, dir, FULL);
		if (q != ((void *)0)) {
			(G.dot           ) = q;	
			goto repeat;
		}
		
		p = (G.text          );
		if (dir == BACK) {
			p = (G.end           ) - 1;
		}
		q = char_search(p, (G.last_search_pattern) + 1, dir, FULL);
		if (q != ((void *)0)) {	
			(G.dot           ) = q;	
			msg = "search hit BOTTOM, continuing at TOP";
			if (dir == BACK)
				msg = "search hit TOP, continuing at BOTTOM";
		} else
			msg = "Pattern not found";
dc2:
		status_line_bold("%s", msg);
		break;
	case '{':			
		q = char_search((G.dot           ), "\n\n", BACK, FULL);
		if (q != ((void *)0)) {	
			(G.dot           ) = next_line(q);	
		}
		break;
	case '}':			
		q = char_search((G.dot           ), "\n\n", FORWARD, FULL);
		if (q != ((void *)0)) {	
			(G.dot           ) = next_line(q);	
		}
		break;

	}

dc1:
	
	if ((G.end           ) == (G.text          )) {
		char_insert((G.text          ), '\n');	
		(G.dot           ) = (G.text          );
	}
	
	if ((G.dot           ) != (G.end           )) {
		(G.dot           ) = bound_dot((G.dot           ));	
	}

	check_context(c);	


	if (!((unsigned)((c) - '0') <= 9))
		(G.cmdcnt             ) = 0;		
	cnt = (G.dot           ) - begin_line((G.dot           ));
	
	if (*(G.dot           ) == '\n' && cnt > 0 && (G.cmd_mode           ) == CMODE_COMMAND)
		(G.dot           )--;
}

