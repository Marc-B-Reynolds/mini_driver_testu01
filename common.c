// Marc B. Reynolds, 2022-2025
// Public Domain under http://unlicense.org, see link for details.

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

#include <float.h>

#include "mini_testu01.h"



//*****************************************************************************
// example custom hash fuction

// select the compliled in bit finalizer (displayed name in report is name of function)
#define hash wyhash

static inline uint64_t some_other_hash(uint64_t x)
{
  return x;
}


// xor of hi/lo result of a*b
static inline uint64_t wyhash_mx(uint64_t a, uint64_t b)
{
  uint64_t hi,lo;
  
#if !defined(_MSC_VER)
  __uint128_t r = (__uint128_t)a * (__uint128_t)b;
  hi = (uint64_t)(r >> 64);
  lo = (uint64_t)r;
#else
  lo = _umul128(a,b,&hi);
#endif  

  return hi ^ lo;
}

static inline uint64_t wyhash(uint64_t x)
{
#if 1
  return wyhash_mx(x,0xe7037ed1a0b428dbull);
#else  
  __uint128_t t=(__uint128_t)(x^0xe7037ed1a0b428dbull)*(x);    
  x  = (uint64_t)((t>>64)^t);
  x *= 0xa0761d6478bd643;
  x ^= x >> 32;
#endif  
}

static inline uint64_t ur_mum(uint64_t u)
{
  // hack: should be runtime configurable
  static const uint64_t k = 0x8bb84b93962eacc9;
  
  u ^= k;                                               // carryless addition (bijection)
  __uint128_t t = (__uint128_t)u;                       // promotion: (u^k)
  t *= t;                                               // untruncated square: (u^k)^2
  t ^= (t >> 64);                                       // XOR hi and low subresults

  u  = (uint64_t)t;

  u ^= (u >> 32); u *= 0x7fb5d329728ea185;
  
  return u;
}

static inline uint64_t no_ur_mum(uint64_t u)
{
  u  = ur_mum(u);
  u ^= u >> 32;
  
  return u;
}


static inline bool str_eq(char* a, char* b) { return strcmp(a,b) == 0; }


hash_t* get_hash_oneoffs(char* name)
{
  hash_t* h;
  
  // dumb thing until there's enough to care. 
  if (str_eq(name, "wyhash")) {
    h = wyhash;
    bit_finalizer      = h;
    bit_finalizer_name = "hyhash";
    bit_finalizer_type = hash_type_builtin;
    return h;
  }

  if (str_eq(name, "ur_mum")) {
    h = ur_mum;
    bit_finalizer      = h;
    bit_finalizer_name = "ur_mum";
    bit_finalizer_type = hash_type_builtin;
    return h;
  }


  
  return NULL;
}

//*****************************************************************************

void internal_error(char* msg, uint32_t code)
{
  if (code != 0)
    fprintf(stderr, "internal error: %s (%c)\n", msg, code);
  else
    fprintf(stderr, "internal error: %s\n", msg);
  exit(-1);
}

void print_error(char* msg)
{
  fprintf(stderr, "error: %s\n", msg);
}

void print_warning(char* msg)
{
  fprintf(stderr, "warning: %s\n", msg);
}

//*****************************************************************************


xorshift_mul_3_t xorshift_mul_3_gen;

static inline_always uint64_t build_xorshift_mul_3(uint64_t x, const xorshift_mul_3_t* const def)
{
  uint64_t m0 = def->m0;
  uint64_t m1 = def->m1;
  uint32_t s0 = def->s0;
  uint32_t s1 = def->s1;
  uint32_t s2 = def->s2;

  x = (x ^ (x >> s0)) * m0;
  x = (x ^ (x >> s1)) * m1;
  x = (x ^ (x >> s2));

  return x;
}

uint64_t xorshift_mul_3(uint64_t x, const xorshift_mul_3_t* const def)
{
  return build_xorshift_mul_3(x,def);
}


enum {
  mix01,mix02,mix03,mix04,mix05,mix06,mix07,mix08,mix09,mix10,mix11,mix12,mix13,mix14,
  lea01,
  murmur3, // https://github.com/aappleby/smhasher/wiki/MurmurHash3
  xxhash,
  degski
};

// forward decls of expanded function
uint64_t fmix01(uint64_t x);
uint64_t fmix02(uint64_t x);
uint64_t fmix03(uint64_t x);
uint64_t fmix04(uint64_t x);
uint64_t fmix05(uint64_t x);
uint64_t fmix06(uint64_t x);
uint64_t fmix07(uint64_t x);
uint64_t fmix08(uint64_t x);
uint64_t fmix09(uint64_t x);
uint64_t fmix10(uint64_t x);
uint64_t fmix11(uint64_t x);
uint64_t fmix12(uint64_t x);
uint64_t fmix13(uint64_t x);
uint64_t fmix14(uint64_t x);
uint64_t flea01(uint64_t x);

uint64_t fmurmur3(uint64_t x);
uint64_t fxxhash (uint64_t x);
uint64_t fdegski (uint64_t x);

const xorshift_mul_3_t xorshift_mul_3_def[] =
{
  // http://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
  {.s0=31, .m0=0x7fb5d329728ea185, .s1=27, .m1=0x81dadef4bc2dd44d, .s2=33, .name="mix01",  .f=fmix01},
  {.s0=33, .m0=0x64dd81482cbd31d7, .s1=31, .m1=0xe36aa5c613612997, .s2=31, .name="mix02",  .f=fmix02},
  {.s0=31, .m0=0x99bcf6822b23ca35, .s1=30, .m1=0x14020a57acced8b7, .s2=33, .name="mix03",  .f=fmix03},
  {.s0=33, .m0=0x62a9d9ed799705f5, .s1=28, .m1=0xcb24d0a5c88c35b3, .s2=32, .name="mix04",  .f=fmix04},
  {.s0=31, .m0=0x79c135c1674b9add, .s1=29, .m1=0x54c77c86f6913e45, .s2=30, .name="mix05",  .f=fmix05},
  {.s0=31, .m0=0x69b0bc90bd9a8c49, .s1=27, .m1=0x3d5e661a2a77868d, .s2=30, .name="mix06",  .f=fmix06},
  {.s0=30, .m0=0x16a6ac37883af045, .s1=26, .m1=0xcc9c31a4274686a5, .s2=32, .name="mix07",  .f=fmix07},
  {.s0=30, .m0=0x294aa62849912f0b, .s1=28, .m1=0x0a9ba9c8a5b15117, .s2=31, .name="mix08",  .f=fmix08},
  {.s0=32, .m0=0x4cd6944c5cc20b6d, .s1=29, .m1=0xfc12c5b19d3259e9, .s2=32, .name="mix09",  .f=fmix09},
  {.s0=30, .m0=0xe4c7e495f4c683f5, .s1=32, .m1=0xfda871baea35a293, .s2=33, .name="mix10",  .f=fmix10},
  {.s0=27, .m0=0x97d461a8b11570d9, .s1=28, .m1=0x02271eb7c6c4cd6b, .s2=32, .name="mix11",  .f=fmix11},
  {.s0=29, .m0=0x3cd0eb9d47532dfb, .s1=26, .m1=0x63660277528772bb, .s2=33, .name="mix12",  .f=fmix12},
  {.s0=30, .m0=0xbf58476d1ce4e5b9, .s1=27, .m1=0x94d049bb133111eb, .s2=31, .name="mix13",  .f=fmix13},
  {.s0=30, .m0=0x4be98134a5976fd3, .s1=29, .m1=0x3bc0993a5ad19a13, .s2=31, .name="mix14",  .f=fmix14},

  {.s0=32, .m0=0xdaba0b6eb09322e3, .s1=32, .m1=0xdaba0b6eb09322e3, .s2=32, .name="lea01",  .f=flea01},
  {.s0=33, .m0=0xff51afd7ed558ccd, .s1=33, .m1=0xc4ceb9fe1a85ec53, .s2=33, .name="murmur3",.f=fmurmur3},
  {.s0=33, .m0=0xc2b2ae3d27d4eb4f, .s1=29, .m1=0x165667b19e3779f9, .s2=32, .name="xxhash", .f=fxxhash},
  {.s0=32, .m0=0xdaba0b6eb09322e3, .s1=32, .m1=0xdaba0b6eb09322e3, .s2=32, .name="degski", .f=fdegski},
};

// code expand
uint64_t fmix01(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix01); }
uint64_t fmix02(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix02); }
uint64_t fmix03(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix03); }
uint64_t fmix04(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix04); }
uint64_t fmix05(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix05); }
uint64_t fmix06(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix06); }
uint64_t fmix07(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix07); }
uint64_t fmix08(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix08); }
uint64_t fmix09(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix09); }
uint64_t fmix10(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix10); }
uint64_t fmix11(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix11); }
uint64_t fmix12(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix12); }
uint64_t fmix13(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix13); }
uint64_t fmix14(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+mix14); }
uint64_t flea01(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+lea01); }

uint64_t fmurmur3(uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+murmur3); }
uint64_t fxxhash (uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+xxhash); }
uint64_t fdegski (uint64_t x) { return build_xorshift_mul_3(x, xorshift_mul_3_def+degski); }


//*****************************************************************************

// active function 
hash_t*   bit_finalizer      = hash;  // temp hack
char*     bit_finalizer_name = TOSTRING(hash);
uint32_t  bit_finalizer_type = hash_type_default;
uint32_t  bit_finalizer_id   = (uint32_t)-1;


// dump c code to stdout
void pretty_print_xorshift_mul_3(const xorshift_mul_3_t* def, uint32_t indent)
{
  printf("%*suint64_t %s(uint64_t x)\n"
	 "%*s{\n"
	 "%*s  x = (x ^ (x >> %2u)) * 0x%016lx;\n"
	 "%*s  x = (x ^ (x >> %2u)) * 0x%016lx;\n"
	 "%*s  x = (x ^ (x >> %2u));\n"
	 "%*s  return x;\n"
	 "%*s}\n",
	 indent,"",def->name,
	 indent,"",
	 indent,"",def->s0,def->m0,
	 indent,"",def->s1,def->m1,
	 indent,"",def->s2,
	 indent,"",
	 indent,"");
}

// list hardcoded parameter versions
void print_xorshift_mul_3(void)
{
  printf("{%s", xorshift_mul_3_def[0].name);
  for(uint32_t i=1; i<LENGTHOF(xorshift_mul_3_def); i++) {
    printf(",%s", xorshift_mul_3_def[i].name);
  }
  printf("}\n");
}

const xorshift_mul_3_t* xorshift_mul_3_current = NULL;

hash_t* get_xorshift_mul_3(char* name)
{
  for(uint32_t i=0; i<LENGTHOF(xorshift_mul_3_def); i++) {
    if (strcmp(name,xorshift_mul_3_def[i].name) == 0) {
      xorshift_mul_3_current = xorshift_mul_3_def+i;
      return xorshift_mul_3_def[i].f;
    }
  }

  return NULL;
}



hash_t* get_hash(char* name)
{
  hash_t* h = get_xorshift_mul_3(name);

  if (h) {
    bit_finalizer      = h;
    bit_finalizer_name = name;
    bit_finalizer_type = hash_type_xsm3;
    return h;
  }

  h = get_hash_oneoffs(name);

  if (h) return h;
  
  fprintf(stderr, "warning: hash %s not found. unmodifed\n", name);

  return bit_finalizer;
}


void bit_finalizer_pretty_print(void)
{
}

//*****************************************************************************

uint64_t get_timestamp(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
