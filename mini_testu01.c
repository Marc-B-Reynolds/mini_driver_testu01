// Marc B. Reynolds, 2022-2025
// Public Domain under http://unlicense.org, see link for details.

// 
// A very hacky TestU01 driver for examining bit finalizers
// in absence of having specialized testing libraries. 
//
// TestU01 is getting "very long in tooth". It's designed to
// test 32-bit integers where a number of tests drop 'n' of
// the high bits of these 32. So to shoehorn in 64-bit
// generators we want to run test with both the lower 32-bits
// being fed in and also with the bit-reversed output.
//
// 1) modify hash function or fill binary file
// 2) compile: {cc} -O3 -march=native mini_testu01.c -o mini_testu01 -lmylib -ltestu01
// 3) profit! (run with --help)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "util.h"
#include "unif01.h"
#include "swrite.h"
#include "bbattery.h"

#include <float.h>

#if !defined(_MSC_VER)
#define UNUSED __attribute__((unused))
#else
#define UNUSED 
#endif




uint64_t seed = 1;

#if 0

static inline uint64_t wyrand(uint64_t x)
{    
  seed+=0xa0761d6478bd642full;    
  __uint128_t	t=(__uint128_t)(seed^0xe7037ed1a0b428dbull)*(seed);    
  return	(t>>64)^t;    
}
#endif

static const uint64_t xprng_mul_k = UINT64_C(0xd1342543de82ef95);
static const uint64_t xprng_add_k = UINT64_C(0x2545f4914f6cdd1d);


static inline uint64_t pcg(uint64_t x)
{
  x = seed;
  seed = xprng_mul_k*seed + xprng_add_k;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
  x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
  x = (x ^ (x >> 31));
  return x;
}


typedef struct { uint64_t state[3]; } prng_t;
enum { PRNG_LCG_0, PRNG_XGB_L, PRNG_XGB_H, PRNG_LENGTH };

prng_t prng_state;
prng_t* prng = & prng_state;

static inline uint64_t prng_mix_64(uint64_t x)
{
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
  x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
  x = (x ^ (x >> 31));
  return x;
}

static inline uint64_t prng_rot(uint64_t x, uint32_t n) { n &= 0x3f; return (x<<n) | (x>>(-n & 0x3f)); }

static inline uint64_t prng_u64(UNUSED uint64_t x)
{
  uint64_t s0 = prng->state[PRNG_XGB_L];
  uint64_t s1 = prng->state[PRNG_XGB_H];
  uint64_t s2 = prng->state[PRNG_LCG_0];
  uint64_t r  = prng_mix_64(s0 + s2);
  
  s1 ^= s0;
  prng->state[PRNG_LCG_0] = xprng_mul_k * s2 + xprng_add_k;
  prng->state[PRNG_XGB_L] = prng_rot(s0,55) ^ s1 ^ (s1<<14);
  prng->state[PRNG_XGB_H] = prng_rot(s1,36);
  
  return r;
}


#include <x86intrin.h>
static inline uint64_t crc32c_64(uint64_t x, uint32_t k) { return _mm_crc32_u64(k,x); }

static inline uint64_t crc32_nl_goof_1(uint64_t x)
{
   x  = crc32c_64(x,0) ^ (x ^ (x >> 9));
   x ^= (x*x) & UINT32_C(~1);
   x ^= x >> 31;
   x *= 0xc6a4a7935bd1e995;
   x  = crc32c_64(x,0) ^ (x ^ (x >> 9));
  return x;
}


// known to be weak: murmurhash64a finalizer
static inline uint64_t murmur2(uint64_t x)
{
  x ^= x >> 47;
  x *= 0xc6a4a7935bd1e995;
  x ^= x >> 47;
  return x;
}


//*****************************************************************************
// common junk between the two programs

#include "mini_testu01.h"


//*****************************************************************************

// glue around hash function

// structure for current state and increment
typedef struct {
  uint64_t counter;
  uint64_t inc;
} data_t;

data_t data = {0};

static inline uint64_t next(void)
{
  uint64_t r = bit_finalizer(data.counter);

  data.counter += data.inc;

  return r;
}

// TestU01 is very dated and was designed to test 32-bit PRNGs.
#if defined(__clang__)
static inline uint64_t bit_reverse_64(uint64_t x) { return __builtin_bitreverse64(x); }
#else

#if !defined(_MSC_VER)
static inline uint64_t byteswap_64(uint64_t x) { return __builtin_bswap64(x); }
#else
// unary minus applied to unsigned type, result is still unsigned
#pragma warning(disable:4146)
static inline uint64_t byteswap_64(uint64_t x) { return _byteswap_uint64(x);  }
#endif

static const uint64_t bit_set_even_1_64  = UINT64_C(0x5555555555555555);
static const uint64_t bit_set_even_2_64  = UINT64_C(0x3333333333333333);
static const uint64_t bit_set_even_4_64  = UINT64_C(0x0f0f0f0f0f0f0f0f);

#define  BIT_GROUP_SWAP(X,L,T) BIT_PERMUTE(X,bit_set_even_ ## L ## _ ## T,L)

static inline uint32_t bit_swap_1_32 (uint32_t x) { return BIT_GROUP_SWAP(x, 1,32); }
static inline uint32_t bit_swap_2_32 (uint32_t x) { return BIT_GROUP_SWAP(x, 2,32); }
static inline uint32_t bit_swap_4_32 (uint32_t x) { return BIT_GROUP_SWAP(x, 4,32); }

static inline uint64_t bit_reverse_64(uint64_t x)
{
  x = byteswap_64(x);
  x = bit_swap_4_64(x);
  x = bit_swap_2_64(x);
  x = bit_swap_1_64(x);
  return x;
}
#endif





static uint64_t next_lo_u32(void* UNUSED p, void* UNUSED s)
{
  return next() & 0xffffffff;
}

static double next_lo_f64(void* UNUSED p, void* UNUSED s)
{
  uint64_t i = next();

  i &= 0x1fffffffffffff;
  
  return (double)i*0x1.0p-53;
}

static uint64_t next_hi_u32(void* UNUSED p, void* UNUSED s)
{
  return next() >> 32;
}

static double next_hi_f64(void* UNUSED p, void* UNUSED s)
{
  uint64_t i = next();

  i >>= (64-53);
  
  return (double)i*0x1.0p-53;
}

static uint64_t next_rev_u32(void* UNUSED p, void* UNUSED s)
{
  return bit_reverse_64(next()) & 0xffffffff;
}

static double next_rev_f64(void* UNUSED p, void* UNUSED s)
{
  uint64_t i = bit_reverse_64(next());

  i &= 0x1fffffffffffff;
  
  return (double)i*0x1.0p-53;
}


//*****************************************************************************
// all ugly temp hacks

// cumulative per stat
uint16_t total_warn[201]  = { 0 };
uint16_t total_error[201] = { 0 };
double   total_peak[201]  = { 0 };

#define ALPHA 0.1
#define BETA 0.00000001
#define ITERATION_LIMIT 1
#define BREAK (ITERATION_LIMIT+1)

void DetectIteration (double pvalue, long *size, int *i)
{
  if (pvalue < BETA || pvalue > 1.0-BETA)
    (*i) = BREAK;
  else if (pvalue < ALPHA || pvalue > 1.0-ALPHA){
    (*size) = 2 * (*size);
    (*i)++;
  }
  else
    (*i) = BREAK;
}

#define RUN_TEST(TEST)                \
 while (i < ITERATION_LIMIT) {        \
   TEST;                              \
   DetectIteration (res->pVal2[gofw_Mean], &size, &i);  \
 }

long get_file_size(const char* filename)
{
  FILE* file = fopen(filename, "rb");
  
  if (file) {
    if (fseek(file, 0, SEEK_END) == 0) {
      long size = ftell(file);
      
      if (size != -1) {
	fclose(file);
	return size;
      }
      printf("error: ftell\n");
    }
    else printf("error: fseek\n");
    
    fclose(file);
  }
  else printf("error: file '%s' not found\n", filename);

   exit(-1);
}

//*****************************************************************************
// copy-pasta-hack

#define HEADER     "\033[95m"
#define OKBLUE     "\033[94m"
#define OKCYAN     "\033[96m"
#define OKGREEN    "\033[92m"
#define WARNING    "\033[93m"
#define FAIL       "\033[91m"
#define ENDC       "\033[0m"
#define BOLD       "\033[1m"
#define UNDERLINE  "\033[4m"

typedef struct {
  char* r;           // right
  char* i;           // interior
  char* d;           // divider
  char* l;           // left
} mini_report_table_style_set_t;


typedef struct {
  mini_report_table_style_set_t t;  // header top
  mini_report_table_style_set_t i;
  mini_report_table_style_set_t b;  //
  char* div;
} mini_report_table_style_t;

// terminal dump styles
mini_report_table_style_t mini_report_table_style_def =
  {
    .t = {.r="┌", .i="─", .d="┬", .l="┐"},
    .i = {.r="├", .i="─", .d="┼", .l="┤"},
    .b = {.r="└", .i="─", .d="┴", .l="┘"},
    .div = "│"
  };

mini_report_table_style_t mini_report_table_style_ascii =
  {
    .t = {.r="+", .i="-", .d="+", .l="+"},
    .i = {.r="+", .i="-", .d="+", .l="|"},
    .b = {.r="+", .i="-", .d="+", .l="+"},
    .div = "|"
  };


#define MINI_REPORT_TABLE_MAX_COL 16

typedef enum {
  mini_report_justify_right,  
  mini_report_justify_left,  
  mini_report_justify_center,  
} mini_report_justify_t;

typedef struct {
  uint32_t flags;
  uint8_t  width;   // total character width
  uint8_t  e_w;     // 
  uint8_t  e_p;     // 
  uint8_t  x2;      // 
  char*    text;    // header text
} mini_report_table_col_t;

typedef struct {
  mini_report_table_style_t* style;
  uint32_t flags;
  uint8_t  num_col;
  uint8_t  left_pad;
  uint8_t  x1;
  uint8_t  x2;
  mini_report_table_col_t col[MINI_REPORT_TABLE_MAX_COL];
} mini_report_table_t;


// super inefficient but who cares
void mini_report_char_rep(FILE* file, char* c, uint32_t n)
{
  while(n--) { fprintf(file,"%s",c); }
}


void mini_report_table_init(mini_report_table_t* table, uint32_t count, ...)
{
  va_list args;
  
  va_start(args, count);
  
  for (uint32_t i=0; i<count; i++) {
    char* text = va_arg(args, char*);
    table->col[i].text  = text;
    table->col[i].width = (uint8_t)strlen(text);
    table->col[i].e_w   = table->col[i].width;
    table->col[i].e_p   = 0;
  }

  table->num_col = (uint8_t)count;
  
  va_end(args);
}

void mini_report_set_col_width(mini_report_table_t* table, uint32_t col, uint32_t w, mini_report_justify_t j)
{
  if (col < table->num_col) {
    uint32_t hlen = (uint32_t)strlen(table->col[col].text);
    if (w > hlen) {
      uint32_t d = w-hlen;
      
      table->col[col].width = (uint8_t)w;

      switch(j) {
      case mini_report_justify_right:
        table->col[col].e_w   = (uint8_t)w;
        table->col[col].e_p   = 0;
	break;

      case mini_report_justify_left:
        table->col[col].e_w   = (uint8_t)(hlen);
        table->col[col].e_p   = (uint8_t)(w-hlen);
	break;

      case mini_report_justify_center:
      default:
	table->col[col].e_w   = (uint8_t)(hlen+(d>>1));
	table->col[col].e_p   = (uint8_t)(w-table->col[col].e_w);
	break;
      }
    }
  }
}


void mini_report_table_div(FILE* file, mini_report_table_t* table, mini_report_table_style_set_t* set)
{
  fprintf(file, "%s", set->r);

  mini_report_char_rep(file, set->i, table->col[0].width);

  for(uint32_t c=1; c<table->num_col; c++) {
    fprintf(file, "%s", set->d);
    mini_report_char_rep(file, set->i,table->col[c].width);
  }
  
  fprintf(file, "%s", set->l);
}

void mini_report_table_header(FILE* file, mini_report_table_t* table)
{
  mini_report_table_style_t* style = table->style;

  // top of header
  mini_report_table_div(file, table, &style->t); printf("\n");

  // each of the header entries
  for(uint32_t i=0; i<table->num_col; i++) {
    fprintf(file, "%s" BOLD "%*s" ENDC "%*s",
	    style->div,
	    table->col[i].e_w, table->col[i].text,
	    table->col[i].e_p, ""
	    );
  }
  fprintf(file, "%s\n", style->div);

  // bottom of header
  mini_report_table_div(file, table, &style->i); printf("\n");
}

void mini_report_table_end(FILE* file, mini_report_table_t* table)
{
  mini_report_table_style_t* style = table->style;

  mini_report_table_div(file, table, &style->b); printf("\n");
}

//*****************************************************************************

enum { run_alphabit, run_block, run_rabbit, run_smallcrush, run_crush };

// only 'name' is useful ATM.
typedef struct {
  char*    name;
  uint8_t  num_tests;
  uint8_t  foo;
  uint16_t num_statistics; 
} battery_info_t;

// alphabit and rabbit # of statistics depend on length of data fed:
//   alphabit on [16,17] ?? 
//   rabbit   on [32,38] ??
// would need to go look at the source again.

battery_info_t battery_info[] =
{
  [run_alphabit]   = {.name="Alphabit",       .num_tests= 9, .num_statistics=16},
  [run_block]      = {.name="Block Alphabit", .num_tests= 9, .num_statistics=16},
  [run_rabbit]     = {.name="Rabbit",         .num_tests=26, .num_statistics=32},
  [run_smallcrush] = {.name="SmallCrush",     .num_tests=10, .num_statistics=15},
  [run_crush]      = {.name="Crush",          .num_tests=96, .num_statistics=144},
};

enum { sample_lo, sample_hi, sample_rev      };

typedef struct {
  char*    name;
} sample_info_t;

battery_info_t sample_info[] =
{
  [sample_lo]  = {.name="lower 32-bits" },
  [sample_hi]  = {.name="high 32-bits" },
  [sample_rev] = {.name="bitreverse & truncated to 32-bits" }
};


// a bunch of globals because this is one-shot program. And it
// was grown like a fungus over time.
uint32_t battery = run_alphabit;
uint32_t sample  = sample_lo;
uint32_t trials  = 20;
double   battery_bits = 32.0*1000.0;
char*    filename = NULL;

uint32_t trial_num = 0;
uint32_t statistic_count  = 0;
uint32_t note_count       = 0;
uint32_t suspicious_count = 0;
uint32_t failure_count    = 0;
bool     first_reported   = false;

bool     pvalue_trim    = true;
double   pvalue_report  = 0.01;
double   pvalue_suspect = 0.001;
double   pvalue_fail    = 0x1.0p-40;

int      real_stdout;
int      null_stdout;

//*****************************************************************************
// 

mini_report_table_t table = { .style = &mini_report_table_style_def, .num_col=7 };

void print_pvalue(FILE* file, double p)
{
  double t      = fmin(p, 1.0-p);
  char*  prefix = (t > pvalue_fail) ? ""WARNING : ""FAIL;
  char*  suffix = ""ENDC;

  prefix = (t > pvalue_suspect) ? "" : prefix;
  suffix = (t > pvalue_suspect) ? "" : suffix;

  fprintf(file,"%s%*.*f%s", prefix,10,10,p,suffix);
}

void print_row(uint32_t stat_id)
{
  char* div = table.style->div;

  printf("%s"             // divider
	 "%*u"            // trial number
	 "%s"             // divider
	 "%*u"            // # of the statistic
	 "%s"             // divider
	 " %-*s"          // statistic and parameters (as much as TestU01 fills in)
	 "%s",            // divider

	 div, table.col[0].width,   trial_num,
	 div, table.col[1].width,   stat_id,
	 div, table.col[2].width-1, bbattery_TestNames[stat_id],
	 div
	 );

  // the p-value, closing divider and newline
  print_pvalue(stdout, bbattery_pVal[stat_id]);
  printf("%s\n", div);
}

// per trial report
void report(void)
{
  uint32_t e = (uint32_t)bbattery_NTests;

  statistic_count += e;
  
  for(uint32_t i=0; i<e; i++) {
    double pvalue = bbattery_pVal[i];
    double t      = fmin(pvalue,1.0-pvalue);
    bool   show   = true;

    if (t >= pvalue_report) continue;

    // track worst p-value
    if (t < total_peak[i])
      total_peak[i] = t;
    else if (pvalue_trim) show = false;

    // if first row to be displayed: start the table
    if (!first_reported) {
      printf("\n");
      if (trials > 1) printf("\n" BOLD "TRIALS:" ENDC "\n");
      mini_report_table_header(stdout, &table);
      first_reported = true;
    }

    if (t <= pvalue_suspect) {
      if (t > pvalue_fail) {
	suspicious_count++;
	total_warn[i]++;
      }
      else {
	failure_count++;
	total_error[i]++;
      }
    }
    
    if (show) print_row(i);
  }
}

// cumulative report (WIP)
void report_final(void)
{
  uint32_t e   = (uint32_t)bbattery_NTests;
  char*    div = table.style->div;

  printf("\n" BOLD "TOTALS:" ENDC "\n");
  
  // modify the existing table def since we're done
  mini_report_table_init(&table, 5, "   ","statistic","suspicious","   fail   ", "  worst t   ");
  mini_report_set_col_width(&table, 1, 31, mini_report_justify_center);
  mini_report_table_header(stdout, &table);
  
  for(uint32_t i=0; i<e; i++) {

    // skip statistic with no questionable/fails
    if ((total_warn[i] + total_error[i]) == 0) continue;

    printf("%s"             // divider
	   "%*u"            // # of the statistic
	   "%s"             // divider
	   " %-*s"          // statistic and parameters (as much as TestU01 fills in)
	   "%s"             // divider
	   "%*u"            // suspicious count
	   "%s"             // divider
	   "%*u"            // fail count
	   "%s"             // divider
	   "%e"
	   "%s"             // divider
	   "\n",
	   
	   div, table.col[0].width,   i,
	   div, table.col[1].width-1, bbattery_TestNames[i],
	   div, table.col[2].width,   total_warn[i],
	   div, table.col[3].width,   total_error[i],
	   div, total_peak[i],
	   div
	   );
  }

  mini_report_table_end(stdout, &table);
}



//*****************************************************************************
// TestU01 interface. just globals.

unif01_Gen* gen;

bool testu01out = false;

static void print_state(void* UNUSED s)
{
  printf("  counter = 0x%016lx\n", data.counter);
}

unif01_Gen gen_lo = {
  .name    = "low bits",
  .GetU01  = &next_lo_f64,
  .GetBits = &next_lo_u32,
  .Write   = &print_state
};

unif01_Gen gen_hi = {
  .name    = "hi bits",
  .GetU01  = &next_lo_f64,
  .GetBits = &next_lo_u32,
  .Write   = &print_state
};

unif01_Gen gen_rev = {
  .name    = "bitreversed",
  .GetU01  = &next_rev_f64,
  .GetBits = &next_rev_u32,
  .Write   = &print_state
};

unif01_Gen* gen = &gen_lo;

void help_options(char* name)
{
  printf("Usage: %s [OPTIONS] [FILE]\n", name);
  printf("\n"
	 "\n Batteries:\n" 
	 "  --alphabit[=BLOCKS]  (default)\n"
	 "  --block[=BLOCKS]     block alphabit\n"
	 "  --rabbit[=BLOCKS]    \n"
	 "  --smallcrush         smallcrush file input requires textfile of\n"
	 "                       doubles on [0,1). I've never tried it.\n"
	 "  --crush              crush can't be run on a file\n"
	 "\n p-value limits:     thresholds to display statistic results\n"
	 "  --pshow=[VALUE]      display              (disabled by default)\n" 
	 "  --psus=[VALUE]       report as suspicious (default = 0.001)\n" 
	 "  --pfail=[VALUE]      report as fail       (default = 2^(-40))\n" 
	 "\n TestU01 Output:     any of these disable this program's output\n" 
	 "                       and uses TestU01's internal reporting instead\n" 
	 "  --short                summaries per trial only\n"
	 "  --verbose            \n"
	 "  --collectors         \n"
	 "  --classes            \n"
	 "  --counters           \n"
	 "\n Sampling            only used for non-file runs\n"
	 "  --hi                 upper 32 bits fed to tests\n"
	 "  --lo                 lower 32 bits fed to tests\n"
	 "  --reversed           bit-reversed output fed to tests\n"
	 "  --fundamental        Weyl sequence constant is one (default)\n"
	 "  --increment=VALUE    Weyl sequence constant (odd integer)\n"
	 "  --phi                Weyl sequence constant is golden ratio\n"
	 "  --counter=VALUE      Weyl sequence inital value (default is random)\n"
	 "");

  exit(0);
}


void battery_set(uint32_t id)
{
  battery = id;

  if (optarg) {
    // alphabit & rabbit takes an optional argument
    char*    end;
    uint64_t val = strtoul(optarg, &end, 0);

    if (end[0] == 0) {
      double bits = (double)val * 64.0;

      if (bits >= 512.0)
	battery_bits = bits;
      else {
	printf("blocks=%s ignored. >= 8 required\n", optarg);
      }
    }
    else printf("warning: skipping block argument %s\n", optarg);
  }
}

void parse_options(int argc, char **argv)
{
  static struct option long_options[] = {
    {"alphabit",   optional_argument, 0, 'a'},
    {"block",      optional_argument, 0, 'b'},
    {"rabbit",     optional_argument, 0, 'r'},
    {"smallcrush", no_argument,       0, 's'},
    {"crush",      no_argument,       0, 'c'},
    {"hi",         no_argument,       0, 'h'},
    {"lo",         no_argument,       0, 'l'},
    {"reversed",   no_argument,       0, 'r'},
    {"fundamental",no_argument,       0, 'f'},
    {"phi",        no_argument,       0, 'p'},
    {"counter",    required_argument, 0, 'x'},
    {"increment",  required_argument, 0, 'i'},
    {"trials",     required_argument, 0, 't'},
    {"hash",       optional_argument, 0,  5 },
    
    {"short",      no_argument,       0,  0 },
    {"verbose",    no_argument,       0, 'v'},
    {"parameters", no_argument,       0,  1 },
    {"collectors", no_argument,       0,  2 },
    {"classes",    no_argument,       0,  3 },
    {"counters",   no_argument,       0,  4 },
    
    {"help",       optional_argument, 0, '?'},
    {0,            0,                 0,  0 }
  };
  
  int c;
  
  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "", long_options, &option_index);
    
    if (c == -1)
      break;
    
    switch (c) {
    case 'x':
    case 't':
      {
	char*    end;
	uint64_t val = strtoul(optarg, &end, 0);

	if (c == 't') {
	  if (val != 0) trials = (uint32_t)val;
	}
	else
	  data.counter = val;
      }
      break;

    case 'v':swrite_Basic      = TRUE; testu01out = true;  break;
    case 0:                            testu01out = true;  break;
    case 1:  swrite_Parameters = TRUE; testu01out = true;  break;
    case 2:  swrite_Collectors = TRUE; testu01out = true;  break;
    case 3:  swrite_Classes    = TRUE; testu01out = true;  break;
    case 4:  swrite_Counters   = TRUE; testu01out = true;  break;

    case 5:
      if (optarg) {
	bit_finalizer  = get_hash(optarg);
	break;
      }
      print_xorshift_mul_3();
      exit(0);
      break;
      
    case 'a': battery_set(run_alphabit);         break;
    case 'b': battery_set(run_block);            break;
    case 'r': battery_set(run_rabbit);           break;
    case 's': battery_set(run_smallcrush);       break;
    case 'c': battery_set(run_crush);            break;
    case 'p': data.inc     = 0x9e3779b97f4a7c15; break;
    case '?': help_options(argv[0]);             break;
      
    default:
      printf("internal error: what option? %c (%u)\n", c,c);
    }
  }

  if (optind == argc) return;

  // get filename and silently ignore anything past it
  filename     = argv[optind];
  battery_bits = (double)get_file_size(filename) * 8.0;

  if (battery_bits < 4096.0) {
    printf("error: datafile too small\n");
    exit(-1);
  }
}

void pre_trial(void)
{
  if (!testu01out) dup2(null_stdout, STDOUT_FILENO);
}

void post_trial(void)
{
  dup2(real_stdout, STDOUT_FILENO);
  if (!testu01out) report();
}

int main(int argc, char** argv)
{
  // default to results only
  swrite_Basic = FALSE;

  // default to counting numbers and timestamp based
  // "random" inital value.
  data.inc     = 1;
  data.counter  = get_timestamp();
  data.counter ^= data.counter << 32;
  data.counter *= prng_mul_k;
  data.counter += prng_add_k;
  data.counter ^= data.counter >> 36;

  parse_options(argc, argv);

  // hack-horrific to prevent default TestU01 reporting
  real_stdout = dup(STDOUT_FILENO);
  null_stdout = open("/dev/null", O_WRONLY);
  
  // setup per trial table
  mini_report_table_init(&table, 4, "trial","   ","statistic","p-value");
  mini_report_set_col_width(&table, 2, 31, mini_report_justify_center);
  mini_report_set_col_width(&table, 3, 12, mini_report_justify_center);

  // spew some info about the tests we're performing
  printf("battery: " BOLD "%s" ENDC "\n", battery_info[battery].name);
  printf("source:  ");

  for (uint32_t i=0; i<LENGTHOF(total_peak); i++) {
    total_peak[i] = 1.0;
  }
  
  if (filename) {
    printf("%s : %.0f bits\n", filename, battery_bits);
  }
  else {
    printf("%s\n",   bit_finalizer_name);
    printf("counter: 0x%016lx\n", data.counter);
    printf("inc:     0x%016lx\n", data.inc);
    printf("sample:  %s\n", sample_info[0].name);
    printf("trials:  %u\n", trials);
  }
  
  // file based or internal computation  
  if (filename) {
    if (!testu01out) dup2(null_stdout, STDOUT_FILENO);
    trials = 1;

    switch(battery) {
    case run_alphabit:   bbattery_AlphabitFile(filename, battery_bits);      break;
    case run_block:      bbattery_BlockAlphabitFile(filename, battery_bits); break;

    case run_rabbit:
      battery_bits = 0.25*battery_bits;
      fprintf(stderr, WARNING "warning" ENDC ": halving size because rabbit uses more than specified and barfs\n");
      bbattery_RabbitFile(filename,   battery_bits);
      break;

    case run_smallcrush:
      fprintf(stderr, WARNING "warning" ENDC ": SmallCrush expects a text file of floats. Don't ask me.\n");
      bbattery_SmallCrushFile(filename);
      break;

    case run_crush:
      fprintf(stderr, FAIL "error:" ENDC " Crush doesn't have a file interface\n");
      break;

    default:
      fprintf(stderr, FAIL "internal error:" ENDC " what battery??\n");
      exit(-1);
      break;
    }

    dup2(real_stdout, STDOUT_FILENO);
    if (!testu01out) report();
  }
  else {
    switch(battery) {
    case run_rabbit:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Rabbit(gen, battery_bits);
	post_trial();
      }
      break;
      
    case run_alphabit:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Alphabit(gen, battery_bits, 0, 32);
	post_trial();
      }
      break;
      
    case run_block:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_BlockAlphabit(gen, battery_bits, 0, 32);
	post_trial();
      }
      break;
      
    case run_smallcrush:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_SmallCrush(gen);
	post_trial();
      }
      break;
      
    case run_crush:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Crush(gen);
	post_trial();
      }
      break;

    default:
      printf("internal error: what battery??\n");
      exit(-1);
      break;
    }
  }

  // local multi trial summary information is gathered at per-trial reporting time.
  // can't be bothered to break it out. looking that the testu01 output means your
  // probably looking at some other information anyway.

  if (first_reported) mini_report_table_end(stdout, &table);

  if (!testu01out) {
    if ((suspicious_count+failure_count)==0) {
      printf("result:  " BOLD OKGREEN "passed all" ENDC " %u statistics\n", statistic_count);
    }
    else {
      printf("  statistics:   %10u\n", statistic_count);
      printf("    suspicious: %10u\n", suspicious_count);
      printf("    failed:     %10u\n", failure_count);
      
      if (trials > 1) {
	report_final();
      }
    }
  }
  
  return 0;
}

