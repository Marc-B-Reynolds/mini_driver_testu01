//*****************************************************************************

// compile time constants for: LCG & PCG generators
static const uint64_t prng_mul_k = UINT64_C(0xd1342543de82ef95);
static const uint64_t prng_add_k = UINT64_C(0x2545f4914f6cdd1d);

#define LENGTHOF(X) (sizeof(X)/sizeof(X[0]))
#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)


#if defined(__GNUC__)
#define inline_always inline __attribute__((always_inline))
#else
#define inline_always
#endif

#define hint_pragma(X) _Pragma(#X)

#if defined(__clang__)
#define hint_unroll(X) hint_pragma(clang loop unroll_count(X))
#elif defined(__GNUC__)
#define hint_unroll(X) hint_pragma(GCC unroll X)
#else
#define hint_unroll(X)
#endif

// this is a questionable def but shouldn't break anything
#if defined(__GNUC__)
#define hint_result_barrier(X) __asm__ volatile("" : "+r"(X) : "r"(X));
#else
#define hint_result_barrier(X)
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif
static inline uint64_t hint_no_const_fold_64(uint64_t v)
{
  asm ("" : "+r" (v));
  return v;  
}
#pragma GCC diagnostic pop
#else
static inline uint64_t hint_no_const_fold_64(uint64_t v) { return v; }
#endif

extern void internal_error(char* msg, uint32_t code);
extern void print_error(char* msg);
extern void print_warning(char* msg);


typedef uint64_t (hash_t)(uint64_t);

//*****************************************************************************

enum {
  hash_type_default,      // unknown compiled in
  hash_type_xsm3,         // murmur3 style 2 xorshift/multiply/xorshift
  hash_type_builtin,      // named builtin
};

// active function
//hash_t*   bit_finalizer      = hash;  // temp hack
extern hash_t*   bit_finalizer;
extern char*     bit_finalizer_name;
extern uint32_t  bit_finalizer_type;
extern uint32_t  bit_finalizer_id;

//*****************************************************************************
// hardcoded and general 3 stage xorshift/multiply finalizers
// probably should have X macroed all of this.

typedef struct {
  uint64_t m0,m1;
  uint8_t  s0,s1,s2;
  char*    name;
  hash_t*  f;
} xorshift_mul_3_t;

extern xorshift_mul_3_t xorshift_mul_3_gen;

// dump c code to stdout
extern void pretty_print_xorshift_mul_3(const xorshift_mul_3_t* def, uint32_t indent);

// list hardcoded parameter versions
extern const xorshift_mul_3_t* xorshift_mul_3_current;

extern void print_xorshift_mul_3(void);
extern hash_t* get_xorshift_mul_3(char* name);


//*****************************************************************************
// more duct-tape and glue!

static inline uint64_t wyhash(uint64_t x);

extern hash_t* get_hash(char* name);
extern void bit_finalizer_pretty_print(void);

extern uint64_t get_timestamp(void);
