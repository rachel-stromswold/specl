#ifndef READ_H
#define READ_H

#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include "s8.h"

//hints for dynamic buffer sizes
#define ERR_BSIZE		1024
#define SPCL_STR_BSIZE 		1024
#define SPCL_ARGS_BSIZE 	16
#define MAX_NUM_SIZE		10
#define LINE_SIZE 		128
#define ALLOC_LST_N		16
#define MAX_PRINT_ELS		8

//easily find signature lengths
#define SIGLEN(s)		(sizeof(s)/sizeof(valtype))

//hash params
#define DEF_TAB_BITS		4
#define FNV_PRIME		16777619
#define FNV_OFFSET		2166136261
//we grow the hash table when the load factor (number of occupied slots/total slots) is greater than GROW_LOAD_NUM/GROW_LOAD_DEN
#define GROW_LOAD_NUM		4
#define GROW_LOAD_DEN		5

#if SPCL_DEBUG_LVL<1
#define spcl_local static inline
#else
#define spcl_local
#endif

//forward declarations
struct spcl_val;
struct spcl_inst;
struct spcl_uf;
struct spcl_fn_call;

//constants
typedef struct spcl_val (*lib_call)(struct spcl_inst*, struct spcl_fn_call);

typedef enum { E_SUCCESS, E_NOFILE, E_LACK_TOKENS, E_BAD_SYNTAX, E_BAD_VALUE, E_BAD_TYPE, E_NOMEM, E_NAN, E_UNDEF, E_OUT_OF_RANGE, E_ASSERT, N_ERRORS } parse_ercode;
typedef enum {VAL_UNDEF, VAL_ERR, VAL_NUM, VAL_STR, VAL_ARRAY, VAL_MAT, VAL_LIST, VAL_FN, VAL_INST, N_VALTYPES} valtype;
//helper classes and things
typedef enum {BLK_UNDEF, BLK_MISC, BLK_INVERT, BLK_TRANSFORM, BLK_DATA, BLK_ROOT, BLK_COMPOSITE, BLK_FUNC_DEC, BLK_LITERAL, BLK_COMMENT, BLK_SQUARE, BLK_QUOTE, BLK_QUOTE_SING, BLK_PAREN, BLK_CURLY, N_BLK_TYPES} blk_type;

/**
 * Macro to set a value while automagically calculating the string length
 */
#define spcl_set_val(INST, NAME, VAL, COPY) spcl_set_valn(INST, NAME, strlen(NAME), VAL, COPY)
#define make_spcl_fstream(NAME) make_spcl_fstreamn(NAME, strlen(NAME))
/**
 * provides a handy macro which wraps get_sigerr and aborts execution of a function if an invalid signature was detected
 * FN_CALL: the name of the function
 * SIGNATURE: a constant array of valtypes. Each argument in the function FN_CALL is tested to make sure it is of the appropriate type. You may include VAL_UNDEF in this array as a wildcard
 */
#define spcl_sigcheck(FN_CALL, SIGNATURE) \
    spcl_val er = get_sigerr(FN_CALL, SIGLEN(SIGNATURE), SIGLEN(SIGNATURE), SIGNATURE); \
    if (er.type == VAL_ERR) return er
/**
 * Acts like SIGCHECK, but allows for optional arguments.
 * FN_CALL: the name of the function
 * MIN_ARGS: the minimum number of arguments that may be accepted
 * SIGNATURE: the signature of all arguments, including optional ones
 */
#define spcl_sigcheck_opts(FN_CALL, MIN_ARGS, SIGNATURE) \
    spcl_val er = get_sigerr(FN_CALL, MIN_ARGS, SIGLEN(SIGNATURE), SIGNATURE); \
    if (er.type == VAL_ERR) return er
/**
 * register a function FN_CALL to the spcl_inst CON with the name NAME
 * CON: the spcl_inst to add the function to
 * FN_CALL: the C function to add
 * NAME: the name of the function when calling from a spcl script
 */
#define spcl_add_fn(INST, FN_CALL, NAME) spcl_set_valn(INST, NAME, strlen(NAME), spcl_make_fn(NAME, 1, &FN_CALL), 0);

/** ======================================================== utility functions ======================================================== **/

/**
 * Works similarly to strncmp, but ignores leading and tailing whitespace and returns a negative spcl_val if strlen(b)<n or a positive spcl_val if strlen(b)>n
 * a: the first string
 * b: the second string
 * n: match at most n characters
 */
int namecmp(const char* a, const char* b, size_t n);
/**
 * write at most n bytes of the double spcl_vald x to str
 */
int write_numeric(char* str, size_t n, double x);

/** ============================ spcl_fstream ============================ **/

typedef struct spcl_fstream {
    FILE* f;		//the file pointer to read from
    psize flen;		//the length of the file
    psize cst;		//the starting location of the cache in f. This is computed using ftell(f).
    psize clen;		//the length of the cache in bytes
    char* cache;	//a cache of the fstream near a given location
} spcl_fstream;
/**
 * Return a new spcl_fstream
 * p_fname: the filename to read
 * n: the length of the filename in bytes.
 */
spcl_fstream* make_spcl_fstreamn(const char* p_fname, size_t n);
/**
 * Free memory associated with the spcl_fstream fs.
 * fs: the fstream to destroy. This should be created with a call to make_spcl_fstream or copy_spcl_fstream as a call to free(fs) is made.
 */
void destroy_spcl_fstream(spcl_fstream* fs);
/**
 * Find the line that the location s resides on.
 */
psize fs_find_line(const spcl_fstream* fs, psize s);
/**
 * Find the index of the end of the file.
 */
psize fs_end(const spcl_fstream* fs);
/**
 * Find the first line end after the index s.
 */
psize fs_line_end(const spcl_fstream* fs, psize s);
/**
 * Append the line str to the end of the fstream fs
 * fs: the fstream to modify
 * str: the line to append
 */
void spcl_fstream_append(spcl_fstream* fs, const char* str);
#if SPCL_DEBUG_LVL>0
/**
 * Return a string with the line contained between indices b (inclusive) and e (not inclusive).
 * fs: the spcl_fstream to read
 * b: the beginning to read from
 * e: read up to this character unless e <= b in which case reading goes to the end of the line
 * returns: a string with the contents between b and e. This string should be freed with a call to free().
 */
char* fs_get_line(const spcl_fstream* fs, psize b, psize e, size_t* len);
/**
 * Returns a version of the line buffer which is flattened so that everything fits onto one line.
 * sep_char: each newline in the buffer is replaced by a sep_char, unless sep_char=0 in which no characters are inserted
 * len: a pointer which if not null will hold the length of the string including the null terminator
 */
char* fs_flatten(const spcl_fstream* fs, char sep_char, size_t* len);
#endif

/** ============================ struct spcl_val ============================ **/

typedef struct spcl_error {
    parse_ercode c;
    char msg[ERR_BSIZE];
} spcl_error;

union V {
    spcl_error* e;
    char* s;
    double x;
    double* a;
    struct spcl_val* l;
    struct spcl_uf* f;
    struct spcl_inst* c;
};

struct spcl_val {
    valtype type;
    union V val;
    size_t n_els; //only applicable for string and list types
};
typedef struct spcl_val spcl_val;

/**
 * create an empty spcl_val
 */
spcl_val spcl_make_none();
/**
 * Create a new error with the specified code and format specifier
 * code: error code type
 * format: a format specifier (just like printf)
 * returns: a pointer to an error object with the specified, which should be deallocated with a call to free()
 */
spcl_val spcl_make_err(parse_ercode code, const char* format, ...);
/**
 * create a spcl_val from a float
 */
spcl_val spcl_make_num(double x);
/**
 * create a spcl_val from a string
 */
spcl_val spcl_make_str(const char* s, size_t n);
/**
 * create a spcl_val from a c array of doubles
 */
spcl_val spcl_make_array(double* vs, size_t n);
/**
 * create a spcl_val from a list
 */
spcl_val spcl_make_list(const spcl_val* vs, size_t n_vs);
/**
 * Add a new callable function with the signature sig and function pointer corresponding to the executed code. This function must accept a function and a pointer to an error code and return a spcl_val.
 */
spcl_val spcl_make_fn(const char* name, size_t n_args, spcl_val (*p_exec)(struct spcl_inst*, struct spcl_fn_call));
/**
 * make an instance object with the given type
 * p: the parent of the current instance (i.e. its owner
 * s: the name of the type
 */
spcl_val spcl_make_inst(struct spcl_inst* parent, const char* s);
/**
 * Compare two spcl_vals if appropriate, in a manner similar to strcmp.
 * returns: 0 if a==b, a positive spcl_val if a>b, and a negative spcl_val if a<b. If no comparison is possible, undefined is returned.
 */
spcl_val spcl_valcmp(spcl_val a, spcl_val b);
/**
 * Convert a spcl_val to a string representation.
 * v: the spcl_val to convert to a string
 * buf: the buffer to write to
 * n: The number of bytes in buf which may safely be written
 * returns: a pointer to the null terminator written to buf
 */
char* spcl_stringify(spcl_val v, char* buf, size_t n);
/**
 * Perform a cast of the instance to the type t. An error is returned if a cast is impossible.
 * v: the spcl_val to cast
 * type: the type to cast v to
 * returns: a spcl_val with the specified type or an error spcl_val if the cast was impossible
 */
spcl_val spcl_cast(spcl_val v, valtype type);
/**
 * check if the spcl_val has a type matching the typename str
 */
void cleanup_spcl_val(spcl_val* o);
/**
 * create a new spcl_val which is a deep copy of o
 */
spcl_val copy_spcl_val(const spcl_val o);

/**
 * This function behaves identically to strcmp, except it uses the internal string representatation for speclang. (strings are fat pointers as opposed to null terminated)
 */
int spcl_strcmp(spcl_val a, spcl_val b);

#if SPCL_DEBUG_LVL>0
/**
 * Recursively print out a spcl_val and the spcl_vals it contains. This is useful for debugging.
 */
void print_hierarchy(spcl_val v, FILE* f, size_t depth);
/**
 * Add two spcl_vals together, overwriting the result to l
 * num+num: arithmetic
 * array+array: piecewise addition
 * array+num: add number to each element
 * mat+mat: matrix addition
 * list+*: append to list
 * str+*: append the string representation of the type * to str
 */
// spcl_val operations
void val_add(spcl_val* l, spcl_val r);
/**
 * Add two spcl_vals together, overwriting the result to l
 * num+num: arithmetic
 * array-array: piecewise subtraction
 * array-num: subtract number from each element
 * mat-mat: matrix subtraction
 */
void val_sub(spcl_val* l, spcl_val r);
/**
 * Add two spcl_vals together, overwriting the result to l
 * num*num: arithmetic
 * array*array: piecewise multiplication
 * array*num: multiply each element by a number
 * mat*mat: piecewise matrix multiplication (NOT matrix multiplication)
 */
void val_mul(spcl_val* l, spcl_val r);
/**
 * compute l/r
 * num/num: arithmetic
 * array/array: piecewise division
 * array/num: divide each element by a number
 * mat/mat: piecewise matrix multiplication
 */
void val_div(spcl_val* l, spcl_val r);
/**
 * compute the remainder of l/r
 * num/num: arithmetic
 * array/array: piecewise division
 * array/num: divide each element by a number
 * mat/mat: piecewise matrix multiplication
 */
void val_mod(spcl_val* l, spcl_val r);
/**
 * raise l^r
 * num/num: arithmetic
 * array/array: piecewise division
 * array/num: divide each element by a number
 * mat/mat: piecewise matrix multiplication
 */
void val_exp(spcl_val* l, spcl_val r);
#endif

/** ============================ spcl_fn_call ============================ **/

/**
 * A class which stores a labeled spcl_val.
 */
typedef struct name_val_pair {
    s8 s;	//the name of the pair
    spcl_val v;	//the spcl_val
} name_val_pair;
struct name_val_pair make_name_val_pair(const char* p_name, spcl_val p_val);
void cleanup_name_val_pair(name_val_pair nv);

//TODO: refactor spcl_fn_call to accept psize's instead of names
typedef struct spcl_fn_call {
    s8 name;
    spcl_val args[SPCL_ARGS_BSIZE];
    size_t n_args;
} spcl_fn_call;

spcl_fn_call copy_spcl_fn_call(const spcl_fn_call o);
void cleanup_spcl_fn_call(spcl_fn_call* o);

/** ============================ spcl_inst ============================ **/

struct spcl_inst {
    //members
    name_val_pair* table;
    struct spcl_inst* parent;
    size_t n_memb;
    unsigned char t_bits;//the log base-2 of the size of the table
};
typedef struct spcl_inst spcl_inst;

/**
 * helper struct for struct spcl_inst objects which stores information read from a file
 */
typedef struct read_state {
    const spcl_fstream* b;
    psize start;
    psize end;
} read_state;
/**
 * Constructor for a new spcl_inst initialized with the contents of fname and optional command line arguments
 * fname: the name of the file to read
 * argc: the number arguments
 * argv: an array of arguments taken from the command-line. Note that callers should not directly pass argc,argv from int main(). Rather, argv should only include valid spclang commands. If you know that spclang commands start at the index i, then you should call spcl_inst_from_file(fname, argc-i, argv+(size_t)i).
 * returns: on success, the return type is a VAL_INST and return.val.c is a valid instance. On an error parsing 
 */
spcl_val spcl_inst_from_file(const char* fname, int argc, const char** argv);
/**
 * make an empty spcl_inst. The result must be destroyed using destroy_inst().
 * parent: the parent of this spcl_inst so that we can look up in scope (i.e. a function can access global variables)
 */
struct spcl_inst* make_spcl_inst(spcl_inst* parent);
/**
 * Create a deep copy of the spcl_inst o and return the result. The result must be destroyed using destroy_inst().
 */
struct spcl_inst* copy_spcl_inst(const spcl_inst* o);
/**
 * cleanup the spcl_inst c
 */
void destroy_spcl_inst(spcl_inst* c);
/**
 * Execute the mathematical operation in the string str at the location op_ind
 */
#if SPCL_DEBUG_LVL>0
typedef enum { KEY_NONE, KEY_IMPORT, KEY_CLASS, KEY_IF, KEY_FOR, KEY_ELSE, KEY_WHILE, KEY_BREAK, KEY_CONT, KEY_RET, KEY_FN, SPCL_N_KEYS } spcl_key;
s8 fs_read(const spcl_fstream* fs, psize s, psize e);
spcl_val do_op(struct spcl_inst* c, read_state rs, psize op_loc, psize* new_end, spcl_key key);
read_state make_read_state(const spcl_fstream* fs, psize s, psize e);
spcl_key get_keyword(read_state* rs);
spcl_val find_operator(read_state rs, psize* op_loc, psize* open_ind, psize* close_ind, psize* new_end);
#endif
/**
 * Search the spcl_inst for the variable with the matching name.
 * name: the name of the variable to set
 * returns: the matching spcl_val, no deep copies are performed
 */
spcl_val spcl_find(const struct spcl_inst* c, const char* name);
/**
 * Lookup the object named str in c and save the resulting spcl_inst to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * type: force the object to match the specified typename
 * sto: overwrite this information to save
 * returns: 0 on success or a negative spcl_val if an error occurred (-1 indicates no match, -2 indicates match of the wrong type)
 */
int spcl_find_object(const spcl_inst* c, const char* str, const char* type, spcl_inst** sto);
/**
 * Lookup the spcl_val named str in c and write the first n elements of the resulting list/array to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to. At most n values are written. If the spcl_array found has m elements and m<n, then all values sto[i] with i>=m are not modified.
 * n: the length of sto
 * returns: the number of elements written on success or a negative spcl_val if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_iarray(const spcl_inst* c, const char* str, int* sto, size_t n);
/**
 * Lookup the spcl_val named str in c and write the first n elements of the resulting list/array to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to. At most n values are written. If the spcl_array found has m elements and m<n, then all values sto[i] with i>=m are not modified.
 * n: the length of sto
 * returns: the number of elements written on success or a negative spcl_val if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_uarray(const spcl_inst* c, const char* str, unsigned* sto, size_t n);
/**
 * Lookup the spcl_val named str in c and write the first n elements of the resulting list/array to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to. At most n values are written. If the spcl_array found has m elements and m<n, then all values sto[i] with i>=m are not modified.
 * n: the length of sto
 * returns: the number of elements written on success or a negative spcl_val if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_darray(const spcl_inst* c, const char* str, double* sto, size_t n);
/**
 * Lookup the spcl_val named str in c and write the string sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to
 * n: the length of sto
 * returns: the number of elements written on success or a negative spcl_val if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_str(const spcl_inst* c, const char* str, char* sto, size_t n);
/**
 * lookup the integer spcl_val in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_int(const spcl_inst* c, const char* str, int* sto);
/**
 * lookup the unsigned integer spcl_val in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_uint(const spcl_inst* c, const char* str, unsigned* sto);
/**
 * lookup the floating point spcl_val in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_float(const spcl_inst* c, const char* str, double* sto);
/**
 * Set the spcl_val with a name matching p_name to a copy of p_val.
 * name: the name of the variable to set
 * namelen: the length of the name
 * new_val: the spcl_val to set the variable to
 * copy: This is a boolean which, if true, performs a deep copy of new_val. Otherwise, only a shallow copy (move) is performed.
 * move_assign: If set to true, then the spcl_val is directly moved into the spcl_inst. This can save some time.
 */
void spcl_set_valn(struct spcl_inst* c, const char* name, size_t namelen, spcl_val new_val, int copy);
/**
 * Given a string str, return a spcl_val corresponding to the expression str
 * c: the spcl_inst to use when looking for variables and functions
 * str: the string expression to parse
 * returns: a spcl_val with the resultant expression
 */
spcl_val spcl_parse_line(struct spcl_inst* c, const char* str);
/**
 * Test whether the string str evaluates to true when using c.
 * c: the spcl_inst to use when looking for variables and functions
 * str: the string expression to parse
 * returns: 0 if str evaluated to false or an error occurred during parsing. Otherwise, 1 is returned.
 */
int spcl_test(struct spcl_inst* c, const char* str);
/**
 * Generate a spcl_inst from a list of lines. This spcl_inst will include function declarations, named variables, and subinstances.
 * lines: the array of lines to read from
 * n_lines: the size of the array
 * returns: an error if one was found or an undefined spcl_val on success
 */
spcl_val spcl_read_lines(struct spcl_inst* c, const spcl_fstream* b);

/** ============================ spcl_uf ============================ **/

/**
 * A class for functions defined by the user along with the implementation code
 */
typedef struct spcl_uf {
    spcl_fn_call call_sig;
    read_state code_lines;
    spcl_val (*exec)(spcl_inst*, spcl_fn_call);
    spcl_inst* fn_scope;
} spcl_uf;

/**
 * constructor
 * sig: this specifies the signature of the function used when calling it
 * bufptr: a buffer to be used for line reading
 * n: the number of characters currently in the buffer
 * fp: the file pointer to read from
 */
spcl_uf* make_spcl_uf_ex(spcl_val (*p_exec)(spcl_inst*, spcl_fn_call));
/**
 * create a deep copy of the user_func o and return it. The result must be destroyed using cleanup_user_func.
 */
spcl_uf* copy_spcl_uf(const spcl_uf* o);
/**
 * Dealocate memory used for uf
 */
void destroy_spcl_uf(spcl_uf* uf);
/**
 * evaluate the function
 */
spcl_val spcl_uf_eval(spcl_uf* uf, spcl_inst* c, spcl_fn_call call);

/** ============================ builtin functions ============================ **/
/**
 * Ensure that the function call f has at least min_args arguments and at most max_args. Then ensure that the first f.n_args arguments match the signature sig
 * f: the function call to interpret
 * min_args: f.n_args must be >= min_args or an error is returned
 * max_args: f.n_args must be <= max_args or an error is returned. Undefined behaviour may occur if max_args<min_args.
 * sig: the first f.n_args arguments in f (assuming min_args<=f.n_args<max_args) must match the specified signature. This array must be large enough to hold max_args
 * returns: an error with an appropriate message or undefined if there was no error
 */
spcl_val get_sigerr(spcl_fn_call f, size_t min_args, size_t max_args, const valtype* sig);
/**
 * assert(boolean, optional string message): If the first argument is false, return an error with the specified message. Otherwise, return 1.
 */
spcl_val spcl_assert(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * Return true if the argument is defined, false otherwise.
 */
spcl_val spcl_isdef(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * Get the type of a spcl_val
 */
spcl_val spcl_typeof(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * Make a range following python syntax. If one argument is supplied then a list with tmp_f.args[0] elements is created starting at index 0 and going up to (but not including) tmp_f.args[0]. If two arguments are supplied then the range is from (tmp_f.args[0], tmp_f.args[1]). If three arguments are supplied then the range (tmp_f.args[0], tmp_f.args[1]) is still returned, but now the spacing between successive elements is tmp_f.args[2].
 */
spcl_val spcl_range(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * linspace(a, b, n) Create a list of n equally spaced real numbers starting at a and ending at b. This function must be called with three aguments unlike np.linspace. Note that the spcl_val b is included in the list
 */
spcl_val spcl_linspace(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * Take a list spcl_val and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. spcl_vals are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * spcl_fn_call: the function with arguments passed
 */
spcl_val spcl_flatten(struct spcl_inst* c, spcl_fn_call tmp_f);
/**
 * Take a list spcl_val and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. spcl_vals are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * spcl_fn_call: the function with arguments passed
 */
spcl_val spcl_cat(struct spcl_inst* c, spcl_fn_call tmp_f);
spcl_val spcl_print(struct spcl_inst* c, spcl_fn_call tmp_f);
spcl_val errtype(struct spcl_inst* c, spcl_fn_call tmp_f);

#endif //READ_H
