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
struct context;
struct user_func;
struct func_call;

//constants
typedef struct value (*lib_call)(struct context*, struct func_call);
typedef unsigned int _uint;
typedef unsigned char _uint8;

typedef enum { E_SUCCESS, E_NOFILE, E_LACK_TOKENS, E_BAD_TOKEN, E_BAD_SYNTAX, E_BAD_VALUE, E_BAD_TYPE, E_NOMEM, E_NAN, E_UNDEF, E_OUT_OF_RANGE, N_ERRORS } parse_ercode;
typedef enum {VAL_UNDEF, VAL_ERR, VAL_NUM, VAL_STR, VAL_ARRAY, VAL_MAT, VAL_LIST, VAL_FUNC, VAL_INST, N_VALTYPES} valtype;
const char* const errnames[N_ERRORS] = {"SUCCESS", "FILE_NOT_FOUND", "LACK_TOKENS", "BAD_TOKEN", "BAD_SYNTAX", "BAD_VALUE", "BAD_TYPE", "NOMEM", "NAN", "UNDEF", "OUT_OF_BOUNDS"};
const char* const valnames[N_VALTYPES] = {"undefined", "error", "string", "numeric", "array", "list", "function", "object"};
//helper classes and things
typedef enum {BLK_UNDEF, BLK_MISC, BLK_INVERT, BLK_TRANSFORM, BLK_DATA, BLK_ROOT, BLK_COMPOSITE, BLK_FUNC_DEC, BLK_LITERAL, BLK_COMMENT, BLK_SQUARE, BLK_QUOTE, BLK_QUOTE_SING, BLK_PAREN, BLK_CURLY, N_BLK_TYPES} blk_type;

inline void* xrealloc(void* p, size_t nsize) {
    void* tmp = realloc(p, nsize);
    if (!tmp) {
	fprintf(stderr, "Insufficient memory to allocate block of size %lu!\n", nsize);
	exit(1);
    }
    return tmp;
}

//generic stack class
#define TYPED_A(NAME,TYPE) NAME##TYPE
#define TYPED(NAME,TYPE) TYPED_A(NAME, _##TYPE)
#define WRAP_MATH_FN(FN) value TYPED(fun,FN)(struct context* c, func_call f) {		\
    value sto = check_signature(f, SIGLEN(MATHN_SIG), SIGLEN(MATHN_SIG), MATHN_SIG);	\
    if (sto.type == 0)									\
	return make_val_num( FN(f.args[0].val.val.x) );					\
    cleanup_val(&sto);									\
    sto = check_signature(f, SIGLEN(MATHA_SIG), SIGLEN(MATHA_SIG), MATHA_SIG);		\
    if (sto.type == 0) {								\
	sto.type = VAL_ARRAY;								\
	sto.n_els = f.args[0].val.n_els;						\
	sto.val.a = malloc(sizeof(double)*sto.n_els);					\
	if (!sto.val.a)									\
	    return make_val_error(E_NOMEM, "");						\
	for (size_t i = 0; i < f.args[0].val.n_els; ++i)				\
	    sto.val.a[i] = FN(f.args[0].val.val.a[i]);					\
    }											\
    return sto;										\
}
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

/** ======================================================== utility functions ======================================================== **/

/**
 * This acts similar to getline, but stops at a semicolon, newline (unless preceeded by a \), {, or }.
 * bufptr: a pointer to which the buffer is saved. If bufptr is NULL than a new buffer is allocated through malloc()
 * n: a pointer to a size_t with the number of characters in the buffer pointed to by bufptr. The call will return do nothing and return -1 if n is null but *bufptr is not.
 * fp: file pointer to read from
 * linecount: a pointer to an integer specifying the number of new line characters read.
 * Returns: the number of characters read (including null termination). On reaching the end of the file, 0 is returned.
 */
size_t read_cgs_line(char** bufptr, size_t* n, FILE* fp, size_t* lineno);
/**
 * write at most n bytes of the double valued x to str
 */
int write_numeric(char* str, size_t n, double x);
/**
 * Convert a string separated by the character sep into a list of strings. For instance, if str == "arg1, arg2" and sep == ',' then the output will be a list of the form ["arg1", "arg2"]. If no sep character is found then the list will have one element which contains the whole string.
 * param str: string to parse into a list
 * param sep: separating character to use.
 * param listlen: location to save the length of the returned list to. Note that this pointer MUST be valid. It is not acceptable to call this function with listlen = NULL. (The returned list is not null terminated so this behavior ensures that callers are appropriately able to identify the length of the returned string.)
 * returns: list of strings separated by commas. This should be freed with a call to DTG_free(). In the event of an error, NULL is returned instead.
 * NOTE: The input string str is modified and the returned value uses memory allocated for str. Thus, the caller must ensure that str has a lifetime at least as long as the used string. Users should not try to call free() on any of the strings in the list, only the list itself.
 */
char** csv_to_list(char* str, char sep, size_t* listlen);
/*
 * helper function for parse_value which appends at most n characters from the string str to line while dynamically resizing the buffer if necessary
 * line: the line to save to
 * line_off: the current end of line
 * line_size: the size in memory allocated for line
 * n: the maximum number of characters to write
 */
char* append_to_line(char* line, size_t* line_off, size_t* line_size, const char* str, size_t n);

/** ============================ line_buffer ============================ **/

/**
 * store the position within a line_buffer, both line and offset in the line
 */
typedef struct lbi {
    size_t line;
    size_t off;
} lbi;
lbi make_lbi(size_t pl, size_t po);
lbi lbi_add(lbi lhs, size_t rhs);
lbi lbi_sub(lbi lhs, size_t rhs);
int lbicmp(lbi lhs, lbi rhs);

typedef struct line_buffer {
    char** lines;
    size_t* line_sizes;//length of each line buffer including null terminator
    size_t n_lines;
} line_buffer;
void destroy_line_buffer(line_buffer* lb);
line_buffer* make_line_buffer_file(const char* p_fname);
line_buffer* make_line_buffer_lines(const char** p_lines, size_t pn_lines);
line_buffer* make_line_buffer_sep(const char* line, char sep);
line_buffer* copy_line_buffer(const line_buffer* o);
void lb_split(line_buffer* lb, char split_delim);
#if DEBUG_INFO>0
int it_single(const line_buffer* lb, char** linesto, char start_delim, char end_delim, lbi* start, lbi* end, int* pdepth, int include_delims, int include_start);
#endif
/**
 * Find the line buffer starting on line start_line between the first instance of start_delim and the last instance of end_delim respecting nesting (i.e. if lines={"a {", "b {", "}", "} c"} then {"b {", "}"} is returned. Note that the result must be deallocated with a call to free().
 * start_line: the line to start reading from
 * end_line: if this value is not NULL, then the index of the line on which end_delim was found is stored here. If the end delimeter is not found, then the line is set to n_lines and the offset is set to zero
 * start_delim: the character to be used as the starting delimiter. This needs to be supplied so that we find a matching end_delim at the root level
 * end_delim: the character to be searched for
 * line_offset: the character on line start_line to start reading from, this defaults to zero. Note that this only applies to the start_line, and not any subsequent lines in the buffer.
 * include_delims: if true, then the delimeters are included in the enclosed strings. defualts to false
 * include_start: if true, then the part preceeding the first instance of start_delim will be included. This value is always false if include_delims is false. If include_delims is true, then this defaults to true.
 * returns: a line_buffer object which should be destroyed with a call to destroy_line_buffer().
 */
line_buffer* lb_get_enclosed(const line_buffer* lb, lbi start, lbi* pend, char start_delim, char end_delim, int include_delims, int include_start);
/**
 * Jump to the end of the next enclosed block started with a start_delim character
 * lb: the linebuffer
 * start: the location from which we start seeking
 * start_delim: the starting delimeter to look for (e.g. '(','{'... corresponding to ')','}'... respectively)
 * end_delim: the ending delimiter to look for, see above
 * include_delims: if true, then include the delimeter in the libe buffer
 */
lbi lb_jmp_enclosed(line_buffer* lb, lbi start, char start_delim, char end_delim, int include_delims);
/**
 * Return a string with the line contained between indices b (inclusive) and e (not inclusive).
 * lb: the line_buffer to read
 * b: the beginning to read from
 * e: read up to this character unless e <= b in which case reading goes to the end of the line
 * returns: a string with the contents between b and e. This string should be freed with a call to free().
 */
char* lb_get_line(const line_buffer* lb, lbi b, lbi e);
/**
 * Return the size of the line with index i.
 */
size_t lb_get_line_size(const line_buffer* lb, size_t i);
/**
 * Returns a version of the line buffer which is flattened so that everything fits onto one line.
 * sep_char: each newline in the buffer is replaced by a sep_char, unless sep_char=0 in which no characters are inserted
 * len: a pointer which if not null will hold the length of the string including the null terminator
 */
char* lb_flatten(const line_buffer* lb, char sep_char, size_t* len);
/**
 * move the line buffer forward by one character
 * p: the index to start at
 */
lbi lb_add(const line_buffer* lb, lbi p, size_t rhs);
/**
 * move the line buffer back by one character
 * p: the index to start at
 */
lbi lb_sub(const line_buffer* lb, lbi p, size_t rhs);
/**
 * return how many characters into the line buffer l is
 */
size_t lb_diff(const line_buffer* lb, lbi r, lbi l);
/**
 * returns the character at position pos
 */
char lb_get(const line_buffer* lb, lbi pos);

/** ============================ struct value ============================ **/

typedef struct error {
    parse_ercode c;
    char msg[ERR_BSIZE];
} error;
struct error* make_error(parse_ercode code, const char* format, ...);

union V {
    error* e;
    char* s;
    double x;
    double* a;
    struct value* l;
    struct user_func* f;
    struct context* c;
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
value make_val_undef();
/**
 * Create a new error with the specified code and format specifier
 * code: error code type
 * format: a format specifier (just like printf)
 * returns: a pointer to an error object with the specified, which should be deallocated with a call to free()
 */
value make_val_error(parse_ercode code, const char* format, ...);
/**
 * create a value from a float
 */
value make_val_num(double x);
/**
 * create a value from a string
 */
value make_val_str(const char* s);
/**
 * create a value from a c array of doubles
 */
value make_val_array(double* vs, size_t n);
/**
 * create a value from a list
 */
value make_val_list(const value* vs, size_t n_vs);
/**
 * Add a new callable function with the signature sig and function pointer corresponding to the executed code. This function must accept a function and a pointer to an error code and return a value.
 */
value make_val_func(const char* name, size_t n_args, value (*p_exec)(struct context*, struct func_call));
/**
 * make an instance object with the given type
 * p: the parent of the current instance (i.e. its owner
 * s: the name of the type
 */
value make_val_inst(struct context* parent, const char* s);
/**
 * returns 0 if a and b are equal. If a.type==b.type==numeric then 1 is returned if a>b and -1 is returned if a<b. If a.type==b.type==string then the behavior is identical to the c function strcmp() (i.e. 1 if a is alphabetically after b). On error, -2 is returned.
 */
int value_cmp(value a, value b);
/**
 * returns true if a is a string matching b
 */
int value_str_cmp(value a, const char* b);
/**
 * Convert a value to a string representation.
 * v: the value to convert to a string
 * n: if not NULL, write the number of characters in the string (excluding null terminator)
 * returns: a string which should be deallocated with a call to free()
 */
char* rep_string(value v, size_t* n);
/**
 * Perform a cast of the instance to the type t. An error is returned if a cast is impossible.
 * v: the value to cast
 * type: the type to cast v to
 * returns: a value with the specified type or an error value if the cast was impossible
 */
value cast_to(value v, valtype type);
/**
 * recursively print out a value and the values it contains
 */
void print_hierarchy(value v, FILE* f, size_t depth);
/**
 * check if the value has a type matching the typename str
 */
void cleanup_val(value* o);
/**
 * create a new value which is a deep copy of o
 */
value copy_val(const value o);
/**
 * swap the values stored at a and b
 */
void swap_val(value* a, value* b);
// value operations
/**
 * Add two values together, overwriting the result to l
 * num+num: arithmetic
 * array+array: piecewise addition
 * array+num: add number to each element
 * mat+mat: matrix addition
 * list+*: append to list
 * str+*: append the string representation of the type * to str
 */
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

/** ============================ func_call ============================ **/

/**
 * A class which stores a labeled value.
 */
typedef struct name_val_pair {
    char* name;
    value val;
} name_val_pair;
struct name_val_pair make_name_val_pair(const char* p_name, value p_val);
void cleanup_name_val_pair(name_val_pair nv);

typedef struct func_call {
    char* name;
    name_val_pair args[ARGS_BUF_SIZE];
    size_t n_args;
} func_call;

func_call copy_func(const func_call o);
void cleanup_func(func_call* o);
void swap(func_call* a, func_call* b);
/**
 * Find the named argument with the matching name
 */
value lookup_named(const func_call f, const char* name);

/** ============================ context ============================ **/

struct context {
    //members
    name_val_pair* table;
    struct context* parent;
    size_t n_memb;
    unsigned char t_bits;//the log base-2 of the size of the table
};
typedef struct context context;

/**
 * helper struct for struct context objects which stores information read from a file
 */
typedef struct read_state {
    const line_buffer* b;
    lbi pos;
    lbi end;
    size_t buf_size;
    char* buf;
} read_state;

inline size_t con_size(const context* c) { if (!c) return 0;return 1 << c->t_bits; }
/**
 * make an empty context. The result must be destroyed using destroy_context().
 * parent: the parent of this context so that we can look up in scope (i.e. a function can access global variables)
 */
struct context* make_context(context* parent);
/**
 * Create a deep copy of the context o and return the result. The result must be destroyed using destroy_context().
 */
struct context* copy_context(const context* o);
/**
 * cleanup the context c
 */
void destroy_context(context* c);
/**
 * Execute the mathematical operation in the string str at the location op_ind
 */
value do_op(struct context* c, read_state rs, lbi op_loc);
/**
 * include builtin functions
 * TODO: make this not dumb
 */
void setup_builtins(struct context* c);
/**
 * Search the context for the variable with the matching name.
 * name: the name of the variable to set
 * returns: the matching value, no deep copies are performed
 */
value lookup(const struct context* c, const char* name);
/**
 * Lookup the object named str in c and save the resulting context to sto
 * c: the context to search
 * str: the name to lookup
 * type: force the object to match the specified typename
 * sto: overwrite this information to save
 * returns: 0 on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type)
 */
int lookup_object(const context* c, const char* str, const char* type, context** sto);
/**
 * Lookup the value named str in c and write the first n elements of the resulting list/array to sto
 * c: the context to search
 * str: the name to lookup
 * sto: the array to save to
 * n: the length of sto
 * returns: the number of elements written on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int lookup_c_array(const context* c, const char* str, double* sto, size_t n);
/**
 * Lookup the value named str in c and write the string sto
 * c: the context to search
 * str: the name to lookup
 * sto: the array to save to
 * n: the length of sto
 * returns: the number of elements written on success or a negative value if an error occurred (-1 indicates no match, -2 indicates match of the wrong type, -3 indicates an invalid element)
 */
int lookup_c_str(const context* c, const char* str, char* sto, size_t n);
/**
 * lookup the integer value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int lookup_int(const context* c, const char* str, int* sto);
/**
 * lookup the unsigned integer value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int lookup_size(const context* c, const char* str, size_t* sto);
/**
 * lookup the floating point value in c at str and save to sto.
 * returns: 0 on success or -1 if the name str couldn't be found
 */
int lookup_float(const context* c, const char* str, double* sto);
/**
 * Set the value with a name matching p_name to a copy of p_val
 * name: the name of the variable to set
 * new_val: the value to set the variable to
 * force_push: If set to true, then set_value is guaranteed to increase the stack size by one, even if there is already an element named p_name. This element is guaranteed to receive priority over the existing element, so this may be used to simulate scoped variables.
 * move_assign: If set to true, then the value is directly moved into the context. This can save some time.
 */
void set_value(struct context* c, const char* name, value new_val, int copy);
/**
 * Given the string starting at token, and the index of an open paren parse the result into a func_call struct.
 * token: a c-string which is modified in place that contains the function
 * open_par_ind: the location of the open parenthesis
 * f: the func_call that information should be saved to
 * end: If not NULL, a pointer to the first character after the end of the string is stored here. If an error occurred during parsing end will be set to NULL.
 * name_only: if set to true, then only the names of the function arguments will be set and all values will be set to undefined. This is useful for handling function declarations
 * returns: an errorcode if an invalid string was supplied.
 */
func_call parse_func(struct context* c, char* token, long open_par_ind, value* v_er, char** end, int name_only);
/**
 * Given a string str (which will be modified by this call), return a value corresponding to the expression str
 * c: the context to use when looking for variables and functions
 * str: the string expression to parse
 * returns: a value with the resultant expression
 */
value parse_value(struct context* c, char* str);
/**
 * Generate a context from a list of lines. This context will include function declarations, named variables, and subcontexts (instances).
 * lines: the array of lines to read from
 * n_lines: the size of the array
 * returns: an error if one was found or an undefined value on success
 */
value read_from_lines(struct context* c, const line_buffer* b);

/** ============================ user_func ============================ **/

/**
 * A class for functions defined by the user along with the implementation code
 */
typedef struct user_func {
    func_call call_sig;
    line_buffer* code_lines;
    value (*exec)(context*, func_call);
} user_func;

/**
 * constructor
 * sig: this specifies the signature of the function used when calling it
 * bufptr: a buffer to be used for line reading, see read_cgs_line
 * n: the number of characters currently in the buffer, see read_cgs_line
 * fp: the file pointer to read from
 */
user_func* make_user_func_lb(func_call sig, line_buffer* b);
/**
 * constructor
 * sig: this specifies the signature of the function used when calling it
 * bufptr: a buffer to be used for line reading, see read_cgs_line
 * n: the number of characters currently in the buffer, see read_cgs_line
 * fp: the file pointer to read from
 */
user_func* make_user_func_ex(value (*p_exec)(context*, func_call));
/**
 * create a deep copy of the user_func o and return it. The result must be destroyed using cleanup_user_func.
 */
user_func* copy_user_func(const user_func* o);
/**
 * Dealocate memory used for uf
 */
void destroy_user_func(user_func* uf);
/**
 * evaluate the function
 */
value uf_eval(user_func* uf, context* c, func_call call);

/** ============================ builtin functions ============================ **/
/**
 * Ensure that the function call f has at least min_args arguments and at most max_args. Then ensure that the first f.n_args arguments match the signature sig
 * f: the function call to interpret
 * min_args: f.n_args must be >= min_args or an error is returned
 * max_args: f.n_args must be <= max_args or an error is returned. Undefined behaviour may occur if max_args<min_args.
 * sig: the first f.n_args arguments in f (assuming min_args<=f.n_args<max_args) must match the specified signature. This array must be large enough to hold max_args
 * returns: an error with an appropriate message or undefined if there was no error
 */
value check_signature(func_call f, size_t min_args, size_t max_args, const valtype* sig);
/**
 * Get the type of a value
 */
value get_type(struct context* c, func_call tmp_f);
/**
 * Make a range following python syntax. If one argument is supplied then a list with tmp_f.args[0] elements is created starting at index 0 and going up to (but not including) tmp_f.args[0]. If two arguments are supplied then the range is from (tmp_f.args[0], tmp_f.args[1]). If three arguments are supplied then the range (tmp_f.args[0], tmp_f.args[1]) is still returned, but now the spacing between successive elements is tmp_f.args[2].
 */
value make_range(struct context* c, func_call tmp_f);
/**
 * linspace(a, b, n) Create a list of n equally spaced real numbers starting at a and ending at b. This function must be called with three aguments unlike np.linspace. Note that the value b is included in the list
 */
value make_linspace(struct context* c, func_call tmp_f);
/**
 * Take a list value and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. values are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * func_call: the function with arguments passed
 */
value flatten_list(struct context* c, func_call tmp_f);
/**
 * Take a list value and flatten it so that it has numpy dimensions (n) where n is the sum of the length of each list in the base list. values are copied in order e.g flatten([0,1],[2,3]) -> [0,1,2,3]
 * func_call: the function with arguments passed
 */
value concatenate(struct context* c, func_call tmp_f);
value print(struct context* c, func_call tmp_f);
value fun_sin(struct context* c, func_call tmp_f);
value fun_cos(struct context* c, func_call tmp_f);
value fun_tan(struct context* c, func_call tmp_f);
value fun_exp(struct context* c, func_call tmp_f);
value fun_sqrt(struct context* c, func_call tmp_f);
value errtype(struct context* c, func_call tmp_f);

#endif //READ_H
