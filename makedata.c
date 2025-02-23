// Marc B. Reynolds, 2022-2025
// Public Domain under http://unlicense.org, see link for details.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>

// modifiy this to create test a custom hash function
//uint64_t hash(uint64_t x) { return x; }

#include "mini_testu01.h"

//*****************************************************************************
// default compile time buffer size is 1K which seems small enough to
// buid small files for fast spot checks and enough granularity for
// everything else.
//
// SAC_BUFFER_LEN:   number of 64-bit SAC samples in buffer
// SEQ_BUFFER_LEN:   number of 64-bit integers in buffer
// BUFFER_SIZE:      size in bytes of buffer

#define  SAC_BUFFER_LEN  (2)
#define  SEQ_BUFFER_LEN  (64*SAC_BUFFER_LEN)
#define  BUFFER_SIZE     ( 8*SEQ_BUFFER_LEN)
uint64_t buffer[SEQ_BUFFER_LEN];

// general purpose state data for different sequences
typedef struct {
  uint64_t state;
  uint64_t inc;
} state_t;

state_t sample_state = {.state=0, .inc=1};


//*****************************************************************************
// sequence generators: (u_n)

// additive recurrence (Weyl sequence) for low entropy
static inline_always uint64_t lds_next(state_t* state) { uint64_t r = state->state; state->state += state->inc; return r; }

// LCG for medium entropy (weak low bit behavior)
static inline_always uint64_t lcg_next(state_t* state)
{
  uint64_t r = state->state;
  hint_result_barrier(r);
  state->state = prng_mul_k*r + prng_add_k;
  return r;
}

extern uint64_t fmix13(uint64_t x);

// PCG for high entropy
static inline_always uint64_t pcg_next(state_t* state)
{
  uint64_t s = state->state;
  uint64_t r = fmix13(s);
  hint_result_barrier(r);
  state->state = prng_mul_k*s + prng_add_k;

  return r;
}

//*****************************************************************************


// fills buffer with strict avalanche criterion (SAC) like data:
// for a base sequence 'u_n' h=hash(u_0) and all 64 single bit flips
// to produce 64 values. The sequence 'u_n' cannot be an additive
// recurrence with simple increment value as that will generate many
// repeated output values in buffer causing statistical tests (to
// properly) fail.
static inline_always void sac_buffer_fill(uint64_t (*sample)(state_t*))
{
  for(uint32_t n=0; n<SAC_BUFFER_LEN; n++) {
    uint64_t x = sample(&sample_state);      // x = u_n
    uint64_t h = bit_finalizer(x);           // h = hash(x)
    uint64_t b = 1;                          // b = 2^p  (p=0)
    uint32_t c = 0;                          // c = output index

    hint_unroll(8)
    for(uint32_t p=0; p<64; p++) {
      buffer[c++] = h ^ bit_finalizer(x^b);
      b <<= 1;
    }
  }
}

// fills buffer with hash(u_n)
static inline_always void seq_buffer_fill(uint64_t (*sample)(state_t*))
{
  uint32_t c = 0;
  
  hint_unroll(8)
  for(uint32_t n=0; n<SEQ_BUFFER_LEN; n++) {
    uint64_t x = sample(&sample_state);
    buffer[c++] = bit_finalizer(x);
  }
}

static inline_always void sac_buffer_fill_lds(void) { seq_buffer_fill(lds_next); }
static inline_always void sac_buffer_fill_lcg(void) { seq_buffer_fill(lcg_next); }
static inline_always void sac_buffer_fill_pcg(void) { seq_buffer_fill(pcg_next); }

static inline_always void seq_buffer_fill_lds(void) { seq_buffer_fill(lds_next); }

//*****************************************************************************
// builder to expand sampling & sequence choice

size_t num_blocks = 1;

static inline_always void create_file(const char* filename, void (*fill)(void))
{
  FILE*  file = fopen(filename, "wb");
  size_t t;

  if (file) {
    setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);

    for(size_t i=0; i<num_blocks; i++) {
      fill();
      t = fwrite(buffer, 1, BUFFER_SIZE, file);
      if (t == BUFFER_SIZE) continue;
      // error handling should be here
    }

    fflush(file);
    fclose(file);
  }
  else
    fprintf(stderr, "error: couldn't open '%s'\n", filename);
}




//*****************************************************************************


char* filename = "data.bin";

enum { lds, lcg, pcg };
enum { sac, seq      };

uint32_t sample_type = lds;
uint32_t fill_type   = sac;

void help_options(char* name)
{
  printf("Usage: %s [OPTIONS] FILE\n", name);
  printf("\n"
	 "  output size to produce (default is "   " K)\n"
	 "    --kb=VALUE       kilobytes (2^10)\n"
	 "    --mb=VALUE       megabytes (2^20)\n"
	 "    --gb=VALUE       gigabytes (2^30)\n"
	 "  bit finalizer:     (default is internal)\n"
	 "    --hash=NAME      built-in named hash (no param lists)\n"
	 "  base sequence:     (default is lds)\n"
	 "    --lds=[VALUE]    low    entropy (default VALUE=1)\n"
	 "    --lcg            medium entropy (--sac only)\n"
	 "    --pcg            high   entropy (--sac only)\n"
	 "    --phi            shorthand: --lds=0x9e3779b97f4a7c15\n"
	 "    --state=VALUE    initial state of the sequence\n"
	 "  output type:\n"
	 "    --sac            (default)\n"
	 "    --seq            hash the base sequence\n"
	 "  other:\n"
	 "    --help           \n"
	 "\n");

  exit(0);
}

uint64_t parse_u64(char* str)
{
  char*    end;
  uint64_t val = strtoul(str, &end, 0);
  return val;
}



int main(int argc, char** argv)
{
  static struct option long_options[] = {
    {"kb",         required_argument, 0,  0},
    {"mb",         required_argument, 0,  1},
    {"gb",         required_argument, 0,  2},
    {"sac",        no_argument,       0,  3},
    {"seq",        no_argument,       0,  4},
    {"lds",        optional_argument, 0,  5},
    {"lcg",        no_argument,       0,  6},
    {"pcg",        no_argument,       0,  7},
    {"phi",        no_argument,       0, 'p'},
    {"state",      required_argument, 0, 's'},
    {"hash",       optional_argument, 0, 'h'},
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
      
    case 0: case 1: case 2:
      {
	uint64_t v = parse_u64(optarg);
	v <<= (10*c);
	if (v != 0)
	  num_blocks = v;
      }
      break;

    case 3: case 4: fill_type = (uint32_t)(c-3); break;

    case 5: case 6: case 7:
      sample_type = (uint32_t)(c-5);

      if (optarg) {
	uint64_t v = parse_u64(optarg);
	sample_state.inc = v|1;

	if ((v & 1)==0) {
	  fprintf(stderr, "warning: increment must be odd. setting=0x%016lx\n", v|1); 
	}
      }
      break;

    case 'h':
      if (optarg) {
	bit_finalizer = get_hash(optarg);
	break;
      }
      print_xorshift_mul_3();
      return 0;
      
    case 's': sample_state.state = parse_u64(optarg);  break;
    case 'p': sample_state.inc   = 0x9e3779b97f4a7c15; break;
    case '?': help_options(argv[0]);                   break;
      
    default:
      printf("internal error: what option? %c (%u)\n", c,c);
    }
  }

  // should be left with one argument
  if (optind == argc-1) {

    filename = argv[optind];
    
    if (fill_type == sac) {
      switch(sample_type) {
      case lds: create_file(filename, sac_buffer_fill_lds); return 0;
      case lcg: create_file(filename, sac_buffer_fill_lcg); return 0;
      case pcg: create_file(filename, sac_buffer_fill_pcg); return 0;
      default:
	internal_error("what sampling?", sample_type);
	break;
      }
    }
    else {
      switch(sample_type) {
      case lds: create_file(filename, seq_buffer_fill_lds); return 0;
      case lcg: print_error("random sampling (lcg) "); return -1;
      case pcg: print_error("random sampline (pcg) "); return -1;
      default:
	internal_error("what sampling?", sample_type);
	break;
      }
    }
    return 0;
  }

  print_error("expected a single filename after the options");

  return -1;
}

