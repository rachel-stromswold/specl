#ifndef READ_H
#define READ_H

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//hints for dynamic buffer sizes
#define ERR_BSIZE	1024
#define BUF_SIZE 	1024
#define ARGS_BUF_SIZE 	256
#define MAX_NUM_SIZE	10
#define LINE_SIZE 	128
#define STACK_SIZE	8
#define ALLOC_LST_N	16

//easily find signature lengths
#define SIGLEN(s)	(sizeof(s)/sizeof(valtype))

//hash params
#define DEF_TAB_BITS	4
#define FNV_PRIME	16777619
#define FNV_OFFSET	2166136261
//we grow the hash table when the load factor (number of occupied slots/total slots) is greater than GROW_LOAD_NUM/GROW_LOAD_DEN
#define GROW_LOAD_NUM	4
#define GROW_LOAD_DEN	5

//forward declarations
struct value;
struct spcl_inst;
struct spcl_uf;
struct spcl_func_call;

//constants
typedef struct value (*lib_call)(struct spcl_inst*, struct spcl_func_call);
typedef unsigned int _uint;
typedef unsigned char _uint8;

typedef enum { E_SUCCESS, E_NOFILE, E_LACK_TOKENS, E_BAD_SYNTAX, E_BAD_VALUE, E_BAD_TYPE, E_NOMEM, E_NAN, E_UNDEF, E_OUT_OF_RANGE, E_ASSERT, N_ERRORS } parse_ercode;
const char* const errnames[N_ERRORS] =
{"SUCCESS", "NO_FILE", "LACK_TOKENS", "BAD_SYNTAX", "BAD_VALUE", "BAD_TYPE", "NOMEM", "NAN", "UNDEF", "OUT_OF_BOUNDS", "ASSERT"};
typedef enum {VAL_UNDEF, VAL_ERR, VAL_NUM, VAL_STR, VAL_ARRAY, VAL_MAT, VAL_LIST, VAL_FUNC, VAL_INST, VAL_ANY, N_VALTYPES} valtype;
const char* const valnames[N_VALTYPES] = {"undefined", "error", "numeric", "string", "array", "list", "function", "object"};
//helper classes and things
typedef enum {BLK_UNDEF, BLK_MISC, BLK_INVERT, BLK_TRANSFORM, BLK_DATA, BLK_ROOT, BLK_COMPOSITE, BLK_FUNC_DEC, BLK_LITERAL, BLK_COMMENT, BLK_SQUARE, BLK_QUOTE, BLK_QUOTE_SING, BLK_PAREN, BLK_CURLY, N_BLK_TYPES} blk_type;

//generic stack class
#define TYPED_A(NAME,TYPE) NAME##TYPE
#define TYPED(NAME,TYPE) TYPED_A(NAME, _##TYPE)
#define TYPED3(NAME,TA,TB) TYPED(TYPED_A(NAME,_##TA),TB)
#define PAIR_DEF(TA,TB) struct TYPED3(PAIR,TA,TB) {					\
    TA a;										\
    TB b;										\
} TYPED3(PAIR,TA,TB);									\
struct TYPED3(PAIR,TA,TB) TYPED3(MAKE_PAIR,TA,TB)(TA pa, TB pb) {			\
    struct TYPED3(PAIR,TA,TB) ret;ret.a = pa;ret.b = pb;				\
    return ret;										\
}											\
typedef TYPED3(PAIR,TA,TB) p(TA,TB)
#define stack(TYPE) struct TYPED(STACK,TYPE)
#define STACK_DEF(TYPE) stack(TYPE) {							\
    size_t ptr;										\
    TYPE buf[STACK_SIZE];								\
};											\
stack(TYPE) TYPED(MAKE_STACK,TYPE)() {							\
    stack(TYPE) ret;									\
    ret.ptr = 0;									\
    memset(ret.buf, 0, sizeof(TYPE)*STACK_SIZE);					\
    return ret;										\
}											\
void TYPED(DESTROY_STACK,TYPE) (stack(TYPE)* s, void (*destroy_el)(TYPE*)) {		\
    for (size_t i = 0; i < s->ptr; ++i)							\
	destroy_el(s->buf + i);								\
    s->ptr = 0;										\
}											\
int TYPED(STACK_PUSH,TYPE) (stack(TYPE)* s, TYPE el) {					\
    if (s->ptr == STACK_SIZE)								\
	return 1;									\
    s->buf[s->ptr++] = el;								\
    return 0;										\
}											\
int TYPED(STACK_POP,TYPE)(stack(TYPE)* s, TYPE* sto) {					\
    if (s->ptr == 0 || s->ptr >= STACK_SIZE) return 1;					\
    s->ptr -= 1;									\
    if (sto) *sto = s->buf[s->ptr];							\
    return 0;										\
}											\
int TYPED(STACK_PEEK,TYPE)(stack(TYPE)* s, size_t ind, TYPE* sto) {			\
    if (ind == 0 || ind > s->ptr || ind > STACK_SIZE) return 1;				\
    if (sto) *sto = s->buf[s->ptr-ind];							\
    return 0;										\
}
/**
 * create a pair by shallow copying arguments a and b
 * a: argument to shallow copy
 * b: argument to shallow copy
 */
#define make_p(TA,TB) TYPED3(MAKE_PAIR,TA,TB)
/**
 * Create an empty stack
 */
#define make_stack(TYPE) TYPED(MAKE_STACK,TYPE)
/**
 * Cleanup all memory associated with the stack s.
 * s: the stack to destroy 
 * destroy_el: A callable function which deallocates memory associated for each element. Set to NULL if nothing needs to be done.
 * returns: 0 on success, 1 on failure (i.e. out of memory)
 */
#define destroy_stack(TYPE) TYPED(DESTROY_STACK,TYPE)
/**
 * Push a shallow copy onto the stack.
 * s: the stack to push onto
 * el: the element to push. Ownership is transferred to the stack, so make a copy if you need it
 * returns: 0 on success, 1 on failure (i.e. out of memory)
 */
#define push(TYPE) TYPED(STACK_PUSH,TYPE)
/**
 * Pop the object from the top of the stack and store it to the pointer sto. The caller is responsible for cleaning up any memory after a call to pop. 
 * s: the stack to pop from
 * sto: the location to save to (may be set to NULL)
 * returns: 0 on success, 1 on failure (i.e. popping from an empty stack)
 */
#define pop(TYPE) TYPED(STACK_POP,TYPE) 
/**
 * Examine the object ind indices down from the top of the stack and store it to the pointer sto without removal. Do not deallocate memory for this, as only a shallow copy is returned
 * s: the stack to peek in
 * ind: the index in the stack to read. Note that indices start at 1 as opposed to zero.
 * sto: the location to save to (may be set to NULL)
 * returns: 0 on success, 1 on failure (i.e. popping from an empty stack)
 */
#define peek(TYPE) TYPED(STACK_PEEK,TYPE)

/**
 * Wrap a mathematical function that takes a single floating point argument
 * FN: the function to wrap
 */
#define WRAP_MATH_FN(FN) value TYPED(spcl,FN)(struct spcl_inst* c, spcl_func_call f) {	\
    value sto = get_sigerr(f, SIGLEN(NUM1_SIG), SIGLEN(NUM1_SIG), NUM1_SIG);		\
    if (sto.type == 0)									\
	return spcl_make_num( FN(f.args[0].v.val.x) );					\
    spcl_cleanup_val(&sto);									\
    sto = get_sigerr(f, SIGLEN(ARR1_SIG), SIGLEN(ARR1_SIG), ARR1_SIG);			\
    if (sto.type == 0) {								\
	sto.type = VAL_ARRAY;								\
	sto.n_els = f.args[0].v.n_els;							\
	sto.val.a = malloc(sizeof(double)*sto.n_els);					\
	if (!sto.val.a)									\
	    return spcl_make_err(E_NOMEM, "");						\
	for (size_t i = 0; i < f.args[0].v.n_els; ++i)					\
	    sto.val.a[i] = FN(f.args[0].v.val.a[i]);					\
    }											\
    return sto;										\
}

/**
 * provides a handy macro which wraps get_sigerr and aborts execution of a function if an invalid signature was detected
 * FUNC: the name of the function
 * SIGNATURE: a constant array of valtypes. Each argument in the function FUNC is tested to make sure it is of the appropriate type. You may include VAL_UNDEF in this array as a wildcard
 */
#define spcl_sigcheck(FUNC, SIGNATURE) \
    value er = get_sigerr(FUNC, SIGLEN(SIGNATURE), SIGLEN(SIGNATURE), SIGNATURE); \
    if (er.type == VAL_ERR) return er
/**
 * Acts like SIGCHECK, but allows for optional arguments.
 * FUNC: the name of the function
 * MIN_ARGS: the minimum number of arguments that may be accepted
 * SIGNATURE: the signature of all arguments, including optional ones
 */
#define spcl_sigcheck_opts(FUNC, MIN_ARGS, SIGNATURE) \
    value er = get_sigerr(FUNC, MIN_ARGS, SIGLEN(SIGNATURE), SIGNATURE); \
    if (er.type == VAL_ERR) return er
/**
 * register a function FUNC to the spcl_inst CON with the name NAME
 * CON: the spcl_inst to add the function to
 * FUNC: the C function to add
 * NAME: the name of the function when calling from a spcl script
 */
#define spcl_add_fn(CON, FUNC, NAME) spcl_set_value(CON, NAME, spcl_make_fn(NAME, 1, &FUNC), 0);

/** ======================================================== utility functions ======================================================== **/

/**
 * Works similarly to strncmp, but ignores leading and tailing whitespace and returns a negative value if strlen(b)<n or a positive value if strlen(b)>n
 * a: the first string
 * b: the second string
 * n: match at most n characters
 */
int namecmp(const char* a, const char* b, size_t n);
/**
 * write at most n bytes of the double valued x to str
 */
int write_numeric(char* str, size_t n, double x);

/** ============================ spcl_line_buffer ============================ **/

/**
 * store the position within a spcl_line_buffer, both line and offset in the line
 */
typedef struct lbi {
    size_t line;
    size_t off;
} lbi;
lbi make_lbi(size_t pl, size_t po);
int lbicmp(lbi lhs, lbi rhs);

typedef struct spcl_line_buffer {
    char** lines;
    size_t* line_sizes;//length of each line buffer including null terminator
    size_t n_lines;
} spcl_line_buffer;
spcl_line_buffer* make_spcl_line_buffer(const char* p_fname);
spcl_line_buffer* copy_spcl_line_buffer(const spcl_line_buffer* o);
void destroy_spcl_line_buffer(spcl_line_buffer* lb);
#if SPCL_DEBUG_LVL>0
int it_single(const spcl_line_buffer* lb, char** linesto, char start_delim, char end_delim, lbi* start, lbi* end, int* pdepth, int include_delims, int include_start);
/**
 * Return a string with the line contained between indices b (inclusive) and e (not inclusive).
 * lb: the spcl_line_buffer to read
 * b: the beginning to read from
 * e: read up to this character unless e <= b in which case reading goes to the end of the line
 * returns: a string with the contents between b and e. This string should be freed with a call to free().
 */
char* lb_get_line(const spcl_line_buffer* lb, lbi b, lbi e, size_t* len);
/**
 * Find the line buffer starting on line start_line between the first instance of start_delim and the last instance of end_delim respecting nesting (i.e. if lines={"a {", "b {", "}", "} c"} then {"b {", "}"} is returned. Note that the result must be deallocated with a call to free().
 * start_line: the line to start reading from
 * end_line: if this value is not NULL, then the index of the line on which end_delim was found is stored here. If the end delimeter is not found, then the line is set to n_lines and the offset is set to zero
 * start_delim: the character to be used as the starting delimiter. This needs to be supplied so that we find a matching end_delim at the root level
 * end_delim: the character to be searched for
 * line_offset: the character on line start_line to start reading from, this defaults to zero. Note that this only applies to the start_line, and not any subsequent lines in the buffer.
 * include_delims: if true, then the delimeters are included in the enclosed strings. defualts to false
 * include_start: if true, then the part preceeding the first instance of start_delim will be included. This value is always false if include_delims is false. If include_delims is true, then this defaults to true.
 * returns: a spcl_line_buffer object which should be destroyed with a call to spcl_destroy_line_buffer().
 */
spcl_line_buffer* lb_get_enclosed(const spcl_line_buffer* lb, lbi start, lbi* pend, char start_delim, char end_delim, int include_delims, int include_start);
/**
 * Jump to the end of the next enclosed block started with a start_delim character
 * lb: the linebuffer
 * start: the location from which we start seeking
 * start_delim: the starting delimeter to look for (e.g. '(','{'... corresponding to ')','}'... respectively)
 * end_delim: the ending delimiter to look for, see above
 * include_delims: if true, then include the delimeter in the libe buffer
 */
lbi lb_jmp_enclosed(spcl_line_buffer* lb, lbi start, char start_delim, char end_delim, int include_delims);
/**
 * Returns a version of the line buffer which is flattened so that everything fits onto one line.
 * sep_char: each newline in the buffer is replaced by a sep_char, unless sep_char=0 in which no characters are inserted
 * len: a pointer which if not null will hold the length of the string including the null terminator
 */
char* lb_flatten(const spcl_line_buffer* lb, char sep_char, size_t* len);
#endif

/** ============================ struct value ============================ **/

typedef struct error {
    parse_ercode c;
    char msg[ERR_BSIZE];
} error;

union V {
    error* e;
    char* s;
    double x;
    double* a;
    struct value* l;
    struct spcl_uf* f;
    struct spcl_inst* c;
};

struct value {
    valtype type;
    union V val;
    size_t n_els; //only applicable for string and list types
};
typedef struct value value;

/**
 * create an empty value
 */
value spcl_make_none();
/**
 * Create a new error with the specified code and format specifier
 * code: error code type
 * format: a format specifier (just like printf)
 * returns: a pointer to an error object with the specified, which should be deallocated with a call to free()
 */
value spcl_make_err(parse_ercode code, const char* format, ...);
/**
 * create a value from a float
 */
value spcl_make_num(double x);
/**
 * create a value from a string
 */
value spcl_make_str(const char* s);
/**
 * create a value from a c array of doubles
 */
value spcl_make_array(double* vs, size_t n);
/**
 * create a value from a list
 */
value spcl_make_list(const value* vs, size_t n_vs);
/**
 * Add a new callable function with the signature sig and function pointer corresponding to the executed code. This function must accept a function and a pointer to an error code and return a value.
 */
value spcl_make_fn(const char* name, size_t n_args, value (*p_exec)(struct spcl_inst*, struct spcl_func_call));
/**
 * make an instance object with the given type
 * p: the parent of the current instance (i.e. its owner
 * s: the name of the type
 */
value spcl_make_inst(struct spcl_inst* parent, const char* s);
/**
 * Compare two values if appropriate, in a manner similar to strcmp.
 * returns: 0 if a==b, a positive value if a>b, and a negative value if a<b. If no comparison is possible, undefined is returned.
 */
value spcl_valcmp(value a, value b);
/**
 * Convert a value to a string representation.
 * v: the value to convert to a string
 * n: if not NULL, write the number of characters in the string (excluding null terminator)
 * returns: a string which should be deallocated with a call to free()
 */
char* spcl_stringify(value v, size_t* n);
/**
 * Perform a cast of the instance to the type t. An error is returned if a cast is impossible.
 * v: the value to cast
 * type: the type to cast v to
 * returns: a value with the specified type or an error value if the cast was impossible
 */
value spcl_cast(value v, valtype type);
/**
 * check if the value has a type matching the typename str
 */
void spcl_cleanup_val(value* o);
/**
 * create a new value which is a deep copy of o
 */
value spcl_copy_val(const value o);

#if SPCL_DEBUG_LVL>0
/**
 * Recursively print out a value and the values it contains. This is useful for debugging.
 */
void print_hierarchy(value v, FILE* f, size_t depth);
/**
 * Add two values together, overwriting the result to l
 * num+num: arithmetic
 * array+array: piecewise addition
 * array+num: add number to each element
 * mat+mat: matrix addition
 * list+*: append to list
 * str+*: append the string representation of the type * to str
 */
// value operations
void val_add(value* l, value r);
/**
 * Add two values together, overwriting the result to l
 * num+num: arithmetic
 * array-array: piecewise subtraction
 * array-num: subtract number from each element
 * mat-mat: matrix subtraction
 */
void val_sub(value* l, value r);
/**
 * Add two values together, overwriting the result to l
 * num*num: arithmetic
 * array*array: piecewise multiplication
 * array*num: multiply each element by a number
 * mat*mat: piecewise matrix multiplication (NOT matrix multiplication)
 */
void val_mul(value* l, value r);
/**
 * Add two values together, overwriting the result to l
 * num/num: arithmetic
 * array/array: piecewise division
 * array/num: divide each element by a number
 * mat/mat: piecewise matrix multiplication
 */
void val_div(value* l, value r);
/**
 * raise l^r
 * num/num: arithmetic
 * array/array: piecewise division
 * array/num: divide each element by a number
 * mat/mat: piecewise matrix multiplication
 */
void val_exp(value* l, value r);
#endif

/** ============================ spcl_func_call ============================ **/

/**
 * A class which stores a labeled value.
 */
typedef struct name_val_pair {
    char* s;	//the name of the pair
    value v;	//the value
} name_val_pair;
struct name_val_pair make_name_val_pair(const char* p_name, value p_val);
void cleanup_name_val_pair(name_val_pair nv);

//TODO: refactor spcl_func_call to accept lbi's instead of names
typedef struct spcl_func_call {
    char* name;
    name_val_pair args[ARGS_BUF_SIZE];
    size_t n_args;
} spcl_func_call;

spcl_func_call copy_func_call(const spcl_func_call o);
void spcl_cleanup_func_call(spcl_func_call* o);

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
    const spcl_line_buffer* b;
    lbi start;
    lbi end;
} read_state;
/**
 * Constructor for a new spcl_inst initialized with the contents of fname and optional command line arguments
 * fname: the name of the file to read
 * argc: the number arguments
 * argv: an array of arguments taken from the command-line. Note that callers should not directly pass argc,argv from int main(). Rather, argv should only include valid spclang commands. If you know that spclang commands start at the index i, then you should call spcl_inst_from_file(fname, argc-i, argv+(size_t)i).
 * error: if not NULL, errors are saved to this pointer.
 * returns: a newly created spcl_inst with the contents of fname and argv. The caller is responsible for deallocating memory by calling destroy_inst().
 */
spcl_inst* spcl_inst_from_file(const char* fname, value* error, int argc, char** argv);
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
value do_op(struct spcl_inst* c, read_state rs, lbi op_loc);
#endif
/**
 * Search the spcl_inst for the variable with the matching name.
 * name: the name of the variable to set
 * returns: the matching value, no deep copies are performed
 */
value spcl_find(const struct spcl_inst* c, const char* name);
/**
 * Lookup the object named str in c and save the resulting spcl_inst to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * type: force the object to match the specified typename
 * sto: overwrite this information to save
 * returns: 0 on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type)
 */
int spcl_find_object(const spcl_inst* c, const char* str, const char* type, spcl_inst** sto);
/**
 * Lookup the value named str in c and write the first n elements of the resulting list/array to sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to
 * n: the length of sto
 * returns: the number of elements written on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_array(const spcl_inst* c, const char* str, double* sto, size_t n);
/**
 * Lookup the value named str in c and write the string sto
 * c: the spcl_inst to search
 * str: the name to lookup
 * sto: the array to save to
 * n: the length of sto
 * returns: the number of elements written on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int spcl_find_c_str(const spcl_inst* c, const char* str, char* sto, size_t n);
/**
 * lookup the integer value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_int(const spcl_inst* c, const char* str, int* sto);
/**
 * lookup the unsigned integer value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_size(const spcl_inst* c, const char* str, size_t* sto);
/**
 * lookup the floating point value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int spcl_find_float(const spcl_inst* c, const char* str, double* sto);
/**
 * Set the value with a name matching p_name to a copy of p_val.
 * name: the name of the variable to set
 * new_val: the value to set the variable to
 * copy: This is a boolean which, if true, performs a deep copy of new_val. Otherwise, only a shallow copy (move) is performed.
 * move_assign: If set to true, then the value is directly moved into the spcl_inst. This can save some time.
 */
void spcl_set_value(struct spcl_inst* c, const char* name, value new_val, int copy);
/**
 * Given a string str (which will be modified by this call), return a value corresponding to the expression str
 * c: the spcl_inst to use when looking for variables and functions
 * str: the string expression to parse
 * returns: a value with the resultant expression
 */
value spcl_parse_line(struct spcl_inst* c, char* str);
/**
 * Generate a spcl_inst from a list of lines. This spcl_inst will include function declarations, named variables, and subinstances.
 * lines: the array of lines to read from
 * n_lines: the size of the array
 * returns: an error if one was found or an undefined value on success
 */
value spcl_read_lines(struct spcl_inst* c, const spcl_line_buffer* b);

/** ============================ spcl_uf ============================ **/

/**
 * A class for functions defined by the user along with the implementation code
 */
typedef struct spcl_uf {
    spcl_func_call call_sig;
    spcl_line_buffer* code_lines;
    value (*exec)(spcl_inst*, spcl_func_call);
} spcl_uf;

/**
 * constructor
 * sig: this specifies the signature of the function used when calling it
 * bufptr: a buffer to be used for line reading
 * n: the number of characters currently in the buffer
 * fp: the file pointer to read from
 */
spcl_uf* make_spcl_uf_lb(spcl_func_call sig, spcl_line_buffer* b);
/**
 * constructor
 * sig: this specifies the signature of the function used when calling it
 * bufptr: a buffer to be used for line reading
 * n: the number of characters currently in the buffer
 * fp: the file pointer to read from
 */
spcl_uf* make_spcl_uf_ex(value (*p_exec)(spcl_inst*, spcl_func_call));
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
value spcl_uf_eval(spcl_uf* uf, spcl_inst* c, spcl_func_call call);

/** ============================ builtin functions ============================ **/
/**
 * Ensure that the function call f has at least min_args arguments and at most max_args. Then ensure that the first f.n_args arguments match the signature sig
 * f: the function call to interpret
 * min_args: f.n_args must be >= min_args or an error is returned
 * max_args: f.n_args must be <= max_args or an error is returned. Undefined behaviour may occur if max_args<min_args.
 * sig: the first f.n_args arguments in f (assuming min_args<=f.n_args<max_args) must match the specified signature. This array must be large enough to hold max_args
 * returns: an error with an appropriate message or undefined if there was no error
 */
value get_sigerr(spcl_func_call f, size_t min_args, size_t max_args, const valtype* sig);
/**
 * assert(boolean, optional string message): If the first argument is false, return an error with the specified message. Otherwise, return 1.
 */
value spcl_assert(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * Return true if the argument is defined, false otherwise.
 */
value spcl_isdef(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * Get the type of a value
 */
value spcl_typeof(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * Make a range following python syntax. If one argument is supplied then a list with tmp_f.args[0] elements is created starting at index 0 and going up to (but not including) tmp_f.args[0]. If two arguments are supplied then the range is from (tmp_f.args[0], tmp_f.args[1]). If three arguments are supplied then the range (tmp_f.args[0], tmp_f.args[1]) is still returned, but now the spacing between successive elements is tmp_f.args[2].
 */
value spcl_range(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * linspace(a, b, n) Create a list of n equally spaced real numbers starting at a and ending at b. This function must be called with three aguments unlike np.linspace. Note that the value b is included in the list
 */
value spcl_linspace(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * Take a list value and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. values are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * spcl_func_call: the function with arguments passed
 */
value spcl_flatten(struct spcl_inst* c, spcl_func_call tmp_f);
/**
 * Take a list value and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. values are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * spcl_func_call: the function with arguments passed
 */
value spcl_cat(struct spcl_inst* c, spcl_func_call tmp_f);
value spcl_print(struct spcl_inst* c, spcl_func_call tmp_f);
value errtype(struct spcl_inst* c, spcl_func_call tmp_f);

#endif //READ_H
