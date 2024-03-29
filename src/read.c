#include "read.h"

STACK_DEF(blk_type)
STACK_DEF(size_t)
STACK_DEF(char)

//dumb forward declarations
static inline lbi lb_end(const spcl_line_buffer* lb) {
    return make_lbi(lb->n_lines, 0);
}
/**
 * move the line buffer forward by one character
 * p: the index to start at
 */
static inline lbi lb_add(const spcl_line_buffer* lb, lbi p, size_t rhs) {
    if (p.line >= lb->n_lines)
	return p;
    lbi ret = make_lbi(p.line, p.off+rhs);
    if (ret.off <= lb->line_sizes[ret.line])
	return ret;
    do {
	size_t rem = ret.off - lb->line_sizes[ret.line] - 1;
	if (++ret.line >= lb->n_lines)
	    return make_lbi(lb->n_lines, 0);
	ret.off = rem;
    } while(ret.off >= lb->line_sizes[ret.line]);
    return ret;
}
/**
 * move the line buffer back by one character
 * p: the index to start at
 */
static inline lbi lb_sub(const spcl_line_buffer* lb, lbi p, size_t rhs) {
    if (p.line >= lb->n_lines)
	p.line = lb->n_lines-1;
    //if we stay on the current line we don't need to worry about overflows
    if (p.off >= rhs)
	return make_lbi(p.line, p.off-rhs);
    lbi ret = make_lbi(p.line, p.off);
    while (rhs > ret.off) {
	if (ret.line == 0)
	    return make_lbi(0,0);
	rhs -= p.off;
	ret.off = lb->line_sizes[--ret.line];
    }
    if (ret.off >= rhs)
	ret.off -= rhs;
    return ret;
}
/**
 * returns the character at position pos
 */
static inline char lb_get(const spcl_line_buffer* lb, lbi pos) {
    if (pos.line >= lb->n_lines || pos.off >= lb->line_sizes[pos.line])
        return 0;
    return lb->lines[pos.line][pos.off];
}
/**
 * return how many characters into the line buffer l is
 */
inline size_t lb_diff(const spcl_line_buffer* lb, lbi r, lbi l) {
    size_t ret = 0;
    while (1) {
	ret += r.off;
	if (l.line == r.line)
	    break;
	r.off = lb->line_sizes[--r.line];
    }
    ret -= l.off;
    return ret;
}

/** ======================================================== utility functions ======================================================== **/

/**
 * get the size of a spcl_context
 */
static inline size_t con_size(const spcl_inst* c) {
    if (!c)
	return 0;
    return 1 << c->t_bits;
}

/**
 * iterate through a spcl_inst by finding defined entries in the table
 */
static inline size_t con_it_next(const spcl_inst* c, size_t i) {
    for (; i < con_size(c); ++i) {
	if (c->table[i].v.type)
	    return i;
    }
    return i;
}

/**
 * check if a character is whitespace
 */
static inline int is_whitespace(char c) {
    if (c == 0 || c == ' ' || c == '\t' || c == '\n' || c == '\r')
	return 1;
    return 0;
}

/**
 * Helper function for token_block which tests whether the character c is a token terminator
 */
static inline unsigned char is_char_sep(char c) {
    if (is_whitespace(c) || c == ';' || c == '+'  || c == '-' || c == '*'  || c == '/' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}')
	return 1;
    return 0;
}
/**
 * Helper function to check whether the string determined by read_state is a valid variable name (i.e. it starts with a letter)
 */
static inline int is_name(read_state* rs) {
    while (is_whitespace(lb_get(rs->b, rs->start)) && lbicmp(rs->start, rs->end) < 0)
	rs->start = lb_add(rs->b, rs->start, 1);
    char c = lb_get(rs->b, rs->start);
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int namecmp(const char* a, const char* b, size_t n) {
    size_t i = 0;size_t j = 0;
    while (i < n && is_char_sep(a[i]))
	++i;
    while (j < n && is_char_sep(b[j]))
	++j;
    unsigned char hit_end = 0;// hit_end&1 did a hit a whitespace block? hit_end&2 did b hit a whitespace block?
    while (i < n && j < n) {
	hit_end = is_char_sep(a[i]) | (is_char_sep(b[j]) << 1);
	//if both strings hit the end and we haven't already exited, that means there was a match
	if (hit_end == 3)
	    return 0;
	//if only one string hit the end, then there wasn't a match
	if (hit_end != 0)
	    return 2*hit_end - 3;//since this case implies, hit_end == 1 or 2, this hack tells us whether a or b is larger
	//if we haven't hit the end of either string and there's a mismatch, return the difference
	if (hit_end == 0 && a[i] != b[j])
	    return (int)(a[i] - b[j]);
	++i;
	++j;
    }
    hit_end = is_char_sep(a[i]) | (is_char_sep(b[j]) << 1);
    if (hit_end == 3 || hit_end == 0)
	return 0;
    return 2*hit_end - 3;
}

//check wheter s and e are matching delimeters
static inline char get_match(char s) {
    switch (s) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	case '*': return '*';
	case '\"': return '\"';
	case '\'': return '\'';
	default: return 0;
    }
    return 0;
}

/**
 * Helper function that finds the start of first token before the index s in the read state rs. The returned value is greater than or equal to zero.
 * lb: the line buffer to read from
 * s: the current position
 * stop: do not return any tokens before this index
 */
static inline lbi find_token_before(const spcl_line_buffer* lb, lbi s, lbi stop) {
    int started = 0;
    while (lbicmp(s, stop) > 0) {
	s = lb_sub(lb, s, 1);
	char c = lb_get(lb, s);
	if (!is_whitespace(c))
	    started = 1;
	if ((!started && is_char_sep(c)) || (s.line == 0 && s.off ==0))
	    return s;
    }
    return stop;
}

/**
 * Find the index of the first character c that isn't nested inside a block or NULL if an error occurred
 */
static inline char* strchr_block(char* str, char c) {
    stack(char) blk_stk = make_stack(char)();
    char prev;
    for (size_t i = 0; str[i] != 0; ++i) {
	if (str[i] == c && blk_stk.ptr == 0)
	    return str+i;
	if (str[i] == '(' || str[i] == '[' || str[i] == '{') {
	    if ( push(char)(&blk_stk, str[i]) ) return NULL;
	} else if (str[i] == '}' || str[i] == ']' ||str[i] == ')') {
	    if ( pop(char)(&blk_stk, &prev) || str[i] != get_match(prev) ) return NULL;
	} else if (str[i] == '\"') {
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\"') {
		if ( pop(char)(&blk_stk, NULL) ) return NULL;
	    } else {
		if ( push(char)(&blk_stk, str[i]) ) return NULL;
	    }
	} else if (str[i] == '\'') {
	    //quotes are more complicated
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\'') {
		if ( pop(char)(&blk_stk, NULL) ) return NULL;
	    } else {
		if ( push(char)(&blk_stk, str[i]) ) return NULL;
	    }
	}
    }
    return NULL;
}
static inline lbi strchr_block_rs(const spcl_line_buffer* lb, lbi s, lbi e, char c) {
    stack(char) blk_stk = make_stack(char)();
    char prev;
    char cur = 0;
    while (lbicmp(s, e)) {
	prev = cur;
	cur = lb_get(lb, s);
	if (!cur)
	    break;
	//now look for matches
	if (cur == c && blk_stk.ptr == 0)
	    return s;
	if (cur == '(' || cur == '[' || cur == '{') {
	    if ( push(char)(&blk_stk, cur) ) return e;
	} else if (cur == '}' || cur == ']' ||cur == ')') {
	    if ( pop(char)(&blk_stk, &prev) || cur != get_match(prev) ) return e;
	} else if (cur == '\"') {
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\"') {
		if ( pop(char)(&blk_stk, NULL) ) return e;
	    } else {
		if ( push(char)(&blk_stk, cur) ) return e;
	    }
	} else if (cur == '\'') {
	    //quotes are more complicated
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\'') {
		if ( pop(char)(&blk_stk, NULL) ) return e;
	    } else {
		if ( push(char)(&blk_stk, cur) ) return e;
	    }
	}
	s = lb_add(lb, s, 1);
    }
    return e;
}

/**
 * Find the first instance of a token (i.e. surrounded by whitespace) in the string str which matches comp
 */
static inline lbi token_block(const spcl_line_buffer* lb, lbi s, lbi e, const char* cmp, size_t cmp_len) {
    if (!lb || !cmp)
	return e;
    stack(char) blk_stk = make_stack(char)();
    char prev;
    char cur = 0;
    while (lbicmp(s, e)) {
	prev = cur;
	cur = lb_get(lb, s);
	if (!cur)
	    break;
	//check whether we're at the root level and there was a seperation terminator
	if (blk_stk.ptr == 0 && is_char_sep(prev)) {
	    int found = 1;
	    for (size_t j = 0; j < cmp_len; ++j) {
		//make sure the first cmp_len characters match
		if ( cmp[j] != lb_get(lb, lb_add(lb, s, j)) ) {
		    found = 0;
		    break;
		}
	    }
	    //make sure its ended by a separator
	    if ( found && is_char_sep(lb_get(lb, lb_add(lb, s, cmp_len))) )
		return s;
	}
	if (cur == '(' || cur == '[' || cur == '{') {
	    if ( push(char)(&blk_stk, cur) ) return e;
	} else if (cur == '}' || cur == ']' || cur == ')') {
	    if ( pop(char)(&blk_stk, &prev)  || cur != get_match(prev) ) return e;
	//comments use more then one character
	} else if (prev == '/' && cur == '*') {
	    if ( push(char)(&blk_stk, '*') ) return e;
	} else if (prev == '*' && cur == '/') {
	    if ( pop(char)(&blk_stk, &prev) || cur != get_match(prev) ) return e;
	} else if (prev == '/' && cur == '/') {
	    if ( push(char)(&blk_stk, cur) ) return e;
	//quotes are more complicated 
	} else if (cur == '\"') {
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\"') {
		if ( pop(char)(&blk_stk, NULL) ) return e;
	    } else {
		if ( push(char)(&blk_stk, cur) ) return e;
	    }
	} else if (cur == '\'') {
	    //quotes are more complicated
	    if (blk_stk.ptr != 0 && !peek(char)(&blk_stk, 1, &prev) && prev == '\'') {
		if ( pop(char)(&blk_stk, NULL) ) return e;
	    } else {
		if ( push(char)(&blk_stk, cur) ) return e;
	    }
	}
	s = lb_add(lb, s, 1);
    }
    return e;
}

int write_numeric(char* str, size_t n, double x) {
    if (x >= 1000000)
	return snprintf(str, n, "%e", x);
    return snprintf(str, n, "%f", x);
}

/**
  * Remove the whitespace surrounding a word
  * Note: this function performs trimming "in place"
  * returns: the length of the string including the null terminator
  */
static inline char* trim_whitespace(char* str, size_t* len) {
    if (!str)
	return NULL;
    int started = 0;
    _uint last_non = 0;
    for (_uint i = 0; str[i]; ++i) {
        if (!is_whitespace(str[i])) {
            last_non = i;
            if (!started && i > 0) {
		_uint j = 0;
		for (; str[i+j]; ++j)
		    str[j] = str[i+j];
		str[j] = 0;//we must null terminate
            }
	    started = 1;
        }
    }
    str[last_non+1] = 0;
    if (len) *len = last_non + 1;
    return str;
}

/** ============================ lbi ============================ **/

//create a new line buffer with the given line index and offset
lbi make_lbi(size_t p_line, size_t p_off) {
    lbi ret;
    ret.line = p_line;
    ret.off = p_off;
    return ret;
}

int lbicmp(lbi l, lbi r) {
    if (l.line < r.line)
	return -1;
    if (l.line > r.line)
	return 1;
    if (l.off < r.off)
	return -1;
    if (l.off > r.off)
	return 1;
    return 0;
}

static inline read_state make_read_state(const spcl_line_buffer* lb, lbi s, lbi e) {
    read_state rs;
    rs.b = lb;
    rs.start = s;
    rs.end = e;
    return rs;
}

/** ============================ spcl_line_buffer ============================ **/

/**
 * Return a new spcl_line_buffer
 */
static inline spcl_line_buffer* alloc_line_buffer() {
    spcl_line_buffer* lb = (spcl_line_buffer*)malloc(sizeof(spcl_line_buffer));
    lb->lines = NULL;
    lb->line_sizes = NULL;
    lb->n_lines = 0;
    return lb;
}

/**
 * Free memory associated with the spcl_line_buffer lb
 */
void destroy_spcl_line_buffer(spcl_line_buffer* lb) {
    if (lb->lines) {
        for (size_t i = 0; i < lb->n_lines; ++i)
            free(lb->lines[i]);
        free(lb->lines);
    }
    if (lb->line_sizes)
        free(lb->line_sizes);
    free(lb);
}

spcl_line_buffer* make_spcl_line_buffer(const char* p_fname) {
    spcl_line_buffer* lb = alloc_line_buffer();
    size_t buf_size = LINE_SIZE;
    lb->lines = (char**)malloc(sizeof(char*)*buf_size);
    lb->line_sizes = (size_t*)malloc(sizeof(size_t)*buf_size);
    lb->n_lines = 0;
    FILE* fp = NULL;
    if (p_fname)
        fp = fopen(p_fname, "r");
    if (fp) {
        size_t line_len = 0;
        int go_again = 1;
        do {
	    //reallocate buffer if necessary
            if (lb->n_lines >= buf_size) {
                buf_size *= 2;
                lb->lines = realloc(lb->lines, sizeof(char*)*buf_size);
                lb->line_sizes = realloc(lb->line_sizes, sizeof(size_t)*buf_size);
            }
	    //read the line until a semicolon, newline or EOF is found
            size_t this_size = LINE_SIZE;
            char* this_buf = malloc(this_size);
            int res = fgetc(fp);
            for (line_len = 0; 1; ++line_len) {
                if (line_len >= this_size) {
                    this_size *= 2;
                    this_buf = realloc(this_buf, sizeof(char)*this_size);
                }
                if ((char)res == ';' || (char)res == '\n') {
                    this_buf[line_len] = 0;
                    break;
                } else if (res == EOF) {
		    this_buf[line_len] = 0;
		    go_again = 0;
		    break;
		} else if ((char)res == '/' && line_len > 0 && this_buf[line_len-1] == '/') {
		    this_buf[line_len-1] = 0;
		    break;
		}
                this_buf[line_len] = (char)res;
                res = fgetc(fp);
            }
            if (line_len > 0) {
                this_buf = realloc(this_buf, sizeof(char) * (line_len + 1));
                lb->lines[lb->n_lines] = this_buf;
                lb->line_sizes[lb->n_lines++] = line_len;
            } else {
                free(this_buf);
            }
        } while (go_again);
        lb->lines = realloc(lb->lines, sizeof(char*) * lb->n_lines);
        lb->line_sizes = realloc(lb->line_sizes, sizeof(size_t) * lb->n_lines);
        fclose(fp);
    } else {
        printf("Error: couldn't open file %s for reading!\n", p_fname);
        free(lb->lines);
        lb->lines = NULL;
    }
    return lb;
}

spcl_line_buffer* copy_spcl_line_buffer(const spcl_line_buffer* o) {
    spcl_line_buffer* lb = alloc_line_buffer();
    lb->n_lines = o->n_lines;
    lb->line_sizes = malloc(sizeof(size_t) * lb->n_lines);
    lb->lines = malloc(sizeof(char*) * lb->n_lines);
    for (size_t i = 0; i < lb->n_lines; ++i) {
        lb->lines[i] = strdup(o->lines[i]);
        lb->line_sizes[i] = o->line_sizes[i];
    }
    return lb;
}

//Below are protected functions in line_buffer. They are not intended to be used by external libraries.

/**
 * helper function for get_enclosed and jmp_enclosed. This function reads the line at index k for the line contained between start_delim and end_delim. If only a start_delim is found or start_ind is not NULL, a pointer with a value set to the index of the start of the line is returned.
 * linesto: store the line read into this variable
 * start_delim: the starting delimiter
 * end_delim: the ending delimiter
 * start: the position to start reading. This is updated as soon as the start delimiter is actually found
 * end: the position where reading stopped. This will be at offset 0 on the line after start->line if an end delimiter was not found or the character where reading stopped because of an end delimiter.
 * includ_delims: if true, then the delimiters are included in the string
 * start_ind: a pointer, the value of which is the index in the line k to start reading.
 * depth: keeps track of how many nested pairs of start and end delimeters we've encountered. We only want to exit calling if an end_delim was found. This variable is set to -1 if a zero depth close brace was found to signal that parsing should terminate.
 * end_line: If end delim is set, then the value in end_line is set to k.
 * jp: stores the index where line reading stopped. Either because a NULL terminator was encountered or an end_delim was encountered
 * returns: an index of the position of the index that reading started from or -1 if not found
 */
#if SPCL_DEBUG_LVL==0
static
#endif
int it_single(const spcl_line_buffer* lb, char** linesto, char start_delim, char end_delim, lbi* start, lbi* end, int* pdepth, int include_delims, int include_start) {
    int free_after = 0;
    if (start == NULL) {
        start = (lbi*)malloc(sizeof(lbi));
        start->line = 0;
        start->off = 0;
    }
    int depth = 0;
    if (pdepth)
        depth = *pdepth;
    size_t j = start->off;
    size_t init_off = start->off;
    int ret = -1;
    //iterate through characters in the line looking for an end_delim without a preceeding start_delim
    size_t i = start->line;
    end->line = i;
    while (1) {
        if (lb->lines[i][j] == 0) {
            if (end) {
                end->line = i + 1;
                end->off = j;
            }
            break;
        } else if (lb->lines[i][j] == start_delim) {
	    //this is a special case, depths may only be toggled
            if (end_delim == start_delim) {
                if (j == 0 || lb->lines[i][j - 1] != '\\') {
                    depth = 1 - depth;
                    if (depth == 1) {
			//start block
                        start->line = i;
                        start->off = j;
                        ret = j;
                        if (!include_delims)
                            ++(start->off);
                    } else {
			//end block
                        if (end) {
                            if (include_delims)
                                ++j;
                            end->off = j;
                        }
                        break;
                    }
                }
            } else {
                if (depth == 0) {
                    start->line = i;
                    start->off = j;
                    ret = j;
                    if (!include_delims)
                        ++(start->off);
                }
                ++depth;
            }
        } else if (lb->lines[i][j] == end_delim) {
            --depth;
            if (depth <= 0) {
                if (include_delims)
                    ++j;
                if (end)
                    end->off = j;
                break;
            }
        }
        ++j;
    }
    if (pdepth)
        *pdepth = depth;
    if (linesto) {
        if (include_start)
            *linesto = lb->lines[i] + init_off;
        else
            *linesto = lb->lines[i] + (start->off);
    }
    if (include_start)
        start->off = init_off;
    if (free_after)
        free(start);
    return ret;
}
#if SPCL_DEBUG_LVL<1
static
#endif
char* lb_get_line(const spcl_line_buffer* lb, lbi b, lbi e, size_t* n) {
    /*while (is_whitespace(lb_get(lb, b)))
	b = lb_add(lb, b, 1);*/
    if (b.line >= lb->n_lines) {
        return NULL;
    }
    //figure out how much space we should allocate
    size_t tot_size = lb->line_sizes[b.line] - b.off + 1;
    for (size_t i = b.line+1; i < lb->n_lines && i <= e.line; ++i)
	tot_size += lb->line_sizes[b.line];
    tot_size = tot_size;
    //allocate it and copy
    char* ret = malloc(sizeof(char)*tot_size);
    size_t wi = 0;
    while (lbicmp(b,e) < 0 && wi < tot_size) {
	ret[wi++] = lb_get(lb, b);
	b = lb_add(lb, b, 1);
    }
    ret[wi] = 0;
    if (n) *n = wi;
    return ret;
}
#if SPCL_DEBUG_LVL<1
static
#endif
spcl_line_buffer* lb_get_enclosed(const spcl_line_buffer* lb, lbi start, lbi* pend, char start_delim, char end_delim, int include_delims, int include_start) {
    spcl_line_buffer* ret = alloc_line_buffer();
    //initialization
    if (pend) {
        pend->line = lb->n_lines;
        pend->off = 0;
    }
    if (lb->n_lines == 0) {
        ret->n_lines = 0;
        ret->lines = NULL;
        ret->line_sizes = NULL;
        return ret;
    }
    //set include_start to false if include_delims is false
    include_start &= include_delims;
    ret->lines = (char**)malloc(sizeof(char*) * lb->n_lines);
    ret->line_sizes = (size_t*)malloc(sizeof(size_t) * lb->n_lines);
    //tracking variables
    int depth = 0;

    //iterate through lines
    size_t k = 0;
    size_t start_line = start.line;
    lbi end;
    size_t i = start.line;
    int started = 0;
    for (; depth >= 0 && i < lb->n_lines; ++i) {
        if (lb->lines[i] == NULL) {
            ret->n_lines = i - start_line;
            break;
        } else {
            end.line = i;
            end.off = 0;
            char* this_line;
            int start_ind = it_single(lb, &this_line, start_delim, end_delim, &start, &end, &depth, include_delims, include_start);
            if (start_ind >= 0)
                started = 1;
	    //don't read empty lines
            if (started) {
                ret->line_sizes[k] = end.off - start.off;
                ret->lines[k++] = strndup(this_line, end.off - start.off);
            }
	    //This means that an end delimeter was found. In this case, we need to break out of the loop.
            if (end.line == start.line)
                break;
            start.off = 0;
            ++start.line;
        }
    }
    if (pend)
        *pend = end;
    ret->n_lines = k;
    return ret;
}

#if SPCL_DEBUG_LVL<1
static
#endif
lbi lb_jmp_enclosed(spcl_line_buffer* lb, lbi start, char start_delim, char end_delim, int include_delims) {
    int depth = 0;
    for (size_t i = start.line; depth >= 0 && i < lb->n_lines; ++i) {
        lbi end;
        end.line = i;
        end.off = 0;
        it_single(lb, NULL, start_delim, end_delim, &start, &end, &depth, include_delims, 0);
        if (end.line == start.line)
            return end;
        start.off = 0;
        ++start.line;
    }
    lbi ret;
    ret.line = lb->n_lines;
    ret.off = 0;
    return ret;
}
static inline const char* lb_read(const spcl_line_buffer* lb, lbi s) {
    if (s.line >= lb->n_lines || s.off >= lb->line_sizes[s.line])
	return "";
    return lb->lines[s.line]+s.off;
}
#if SPCL_DEBUG_LVL<1
static
#endif
char* lb_flatten(const spcl_line_buffer* lb, char sep_char, size_t* len) {
    //figure out how much memory must be allocated
    size_t tot_size = 1;
    for (size_t i = 0; i < lb->n_lines; ++i)
        tot_size += lb->line_sizes[i];
    if (sep_char != 0)
        tot_size += lb->n_lines;
    //allocate the memory
    char* ret = (char*)malloc(sizeof(char) * tot_size);
    size_t k = 0;
    for (size_t i = 0; i < lb->n_lines; ++i) {
        for (size_t j = 0; lb->lines[i][j] && j < lb->line_sizes[i]; ++j)
            ret[k++] = lb->lines[i][j];
        if (k != 0 && sep_char != 0)
            ret[k++] = sep_char;
    }
    ret[k] = 0;
    if (len) *len = k+1;
    return ret;
}

/** ======================================================== spcl_func_call ======================================================== **/

void spcl_cleanup_func_call(spcl_func_call* f) {
    if (f) {
	/*if (f->name)
	    free(f->name);*/
	for (size_t i = 0; i < f->n_args; ++i)
	    cleanup_name_val_pair(f->args[i]);
    }
}
spcl_func_call spcl_copy_func_call(const spcl_func_call o) {
    spcl_func_call f;f.name = NULL;
    if (o.name) f.name = strdup(o.name);
    f.n_args = o.n_args;
    for (size_t i = 0; i < f.n_args; ++i) {
	f.args[i].v = spcl_copy_val(o.args[i].v);
	f.args[i].s = (o.args[i].s)? strdup(o.args[i].s) : NULL;
    }
    return f;
}

/** ======================================================== name_val_pair ======================================================== **/

name_val_pair make_name_val_pair(const char* p_name, value p_val) {
    name_val_pair ret;
    ret.s = (p_name) ? strdup(p_name) : NULL;
    ret.v = spcl_copy_val(p_val);
    return ret;
}
void cleanup_name_val_pair(name_val_pair nv) {
    if (nv.s)
	free(nv.s);
    spcl_cleanup_val(&nv.v);
}

/** ======================================================== builtin functions ======================================================== **/
value get_sigerr(spcl_func_call f, size_t min_args, size_t max_args, const valtype* sig) {
    if (!sig || max_args < min_args)
	return spcl_make_none();
    if (f.n_args < min_args)
	return spcl_make_err(E_LACK_TOKENS, "%s() expected %lu arguments, got %lu", f.name, min_args, f.n_args);
    if (f.n_args > max_args)
	return spcl_make_err(E_LACK_TOKENS, "%s() with too many arguments, %lu", f.name, f.n_args);
    for (size_t i = 0; i < f.n_args; ++i) {
	//treat undefined as allowing for arbitrary type
	if (sig[i] && f.args[i].v.type != sig[i]) {
	    //if the type is an error, let it pass through
	    if (f.args[i].v.type == VAL_ERR)
		return f.args[i].v;
	    return spcl_make_err(E_BAD_TYPE, "%s() expected args[%lu].type=%s, got %s", f.name, i, valnames[sig[i]], valnames[f.args[i].v.type]);
	}
	if (sig[i] > VAL_NUM && f.args[i].v.val.s == NULL)
	    return spcl_make_err(E_BAD_TYPE, "%s() found empty %s at args[%lu]", f.name, valnames[sig[i]], i);
    }
    return spcl_make_none();
}
static const valtype ANY1_SIG[] = {VAL_UNDEF};
static const valtype NUM1_SIG[] = {VAL_NUM};
static const valtype ARR1_SIG[] = {VAL_ARRAY};
value spcl_assert(struct spcl_inst*c, spcl_func_call f) {
    static const valtype ASSERT_SIG[] = {VAL_UNDEF, VAL_STR};
    spcl_sigcheck_opts(f, 1, ASSERT_SIG);
    if (f.args[0].v.val.x == 0)
	return (f.n_args == 1)? spcl_make_err(E_ASSERT, "") : spcl_make_err(E_ASSERT, "%s", f.args[1].v.val.s);
    return spcl_make_num(f.args[0].v.val.x);
}
value spcl_typeof(struct spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, ANY1_SIG);
    value sto;
    sto.type = VAL_STR;
    //handle instances as a special case
    if (f.args[0].v.type == VAL_INST) {
	value t = spcl_find(f.args[0].v.val.c, "__type__");
	if (t.type == VAL_STR) {
	    sto.n_els = t.n_els;
	    sto.val.s = malloc(sizeof(char)*(sto.n_els+1));
	    for (size_t i = 0; i < sto.n_els; ++i)
		sto.val.s[i] = t.val.s[i];
	    sto.val.s[sto.n_els] = 0;
	} else {
	    return spcl_make_none();
	}
	return sto;
    }
    sto.n_els = strlen(valnames[f.args[0].v.type])+1;
    sto.val.s = strdup(valnames[f.args[0].v.type]);
    return sto;
}
value spcl_len(struct spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, ANY1_SIG);
    return spcl_make_num(f.args[0].v.n_els);
}
static const valtype RANGE_SIG[] = {VAL_NUM, VAL_NUM, VAL_NUM};
value spcl_range(struct spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck_opts(f, 1, RANGE_SIG);
    double min, max, inc;
    //interpret arguments depending on how many were provided
    if (f.n_args == 1) {
	min = 0;
	max = f.args[0].v.val.x;
	inc = 1;
    } else {
	min = f.args[0].v.val.x;
	max = f.args[1].v.val.x;
	inc = 1;
    }
    if (f.n_args >= 3)
	inc = f.args[2].v.val.x;
    //make sure arguments are valid
    if ((max-min)*inc <= 0)
	return spcl_make_err(E_BAD_VALUE, "range(%f, %f, %f) with invalid increment", min, max, inc);
    value ret;
    ret.type = VAL_ARRAY;
    ret.n_els = (max - min) / inc;
    ret.val.a = malloc(sizeof(double)*ret.n_els);
    for (size_t i = 0; i < ret.n_els; ++i)
	ret.val.a[i] = i*inc + min;
    return ret;
}
static const valtype LINSPACE_SIG[] = {VAL_NUM, VAL_NUM, VAL_NUM};
value spcl_linspace(struct spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, LINSPACE_SIG);
    value ret;
    ret.type = VAL_ARRAY;
    ret.n_els = (size_t)(f.args[2].v.val.x);
    //prevent divisions by zero
    if (ret.n_els < 2)
	return spcl_make_err(E_BAD_VALUE, "cannot make linspace with size %lu", ret.n_els);
    ret.val.a = (double*)malloc(sizeof(double)*ret.n_els);
    double step = (f.args[1].v.val.x - f.args[0].v.val.x)/(ret.n_els - 1);
    for (size_t i = 0; i < ret.n_els; ++i) {
	ret.val.a[i] = step*i + f.args[0].v.val.x;
    }
    return ret;
}
STACK_DEF(value)
static const valtype FLATTEN_SIG[] = {VAL_LIST};
value spcl_flatten(struct spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, FLATTEN_SIG);
    value ret = spcl_make_none();
    value cur_list = f.args[0].v;
    //flattening an empty list is the identity op.
    if (cur_list.n_els == 0 || cur_list.val.l == NULL) {
	ret.type = VAL_LIST;
	return ret;
    }
    size_t cur_st = 0;
    //there may potentially be nested lists, we need to be able to find our way back to the parent and the index once we're done
    stack(value) lists = make_stack(value)();
    stack(size_t) inds = make_stack(size_t)();
    //just make sure that there's a root level on the stack to be popped out
    if ( push(value)(&lists, cur_list) ) return ret;
    if ( push(size_t)(&inds, 0) ) return ret;

    //this is used for estimating the size of the buffer we need. Take however many elements were needed for this list and assume each sub-list has the same number of elements
    size_t base_n_els = cur_list.n_els;
    //start with the number of elements in the lowest order of the list
    size_t buf_size = cur_list.n_els;
    ret.val.l = malloc(sizeof(value)*buf_size);
    size_t j = 0;
    do {
	size_t i = cur_st;
	size_t start_depth = inds.ptr;
	for (; i < cur_list.n_els; ++i) {
	    if (cur_list.val.l[i].type == VAL_LIST) {
		if ( push(value)(&lists, cur_list) ) return ret;
		if ( push(size_t)(&inds, i+1) ) return ret;//push + 1 so that we start at the next index instead of reading the list again
		cur_list = cur_list.val.l[i];
		cur_st = 0;
		break;
	    }
	    if (j >= buf_size) {
		//-1 since we already have at least one element. no base_n_els=0 check is needed since that case will ensure the for loop is never evaluated
		buf_size += (base_n_els-1)*(i+1);
		value* tmp_val = realloc(ret.val.l, sizeof(value)*buf_size);
		if (!tmp_val) {
		    free(ret.val.l);
		    spcl_cleanup_func_call(&f);
		    destroy_stack(value)(&lists, &spcl_cleanup_val);
		    return spcl_make_err(E_NOMEM, "");
		}
		ret.val.l = tmp_val;
	    }
	    ret.val.l[j++] = spcl_copy_val(cur_list.val.l[i]);
	}
	//if we reached the end of a list without any sublists then we should return back to the parent list
	if (inds.ptr <= start_depth) {
	    pop(size_t)(&inds, &cur_st);
	    pop(value)(&lists, &cur_list);
	}
    } while (lists.ptr);
    ret.type = VAL_LIST;
    ret.n_els = j;
    return ret;
}
value spcl_cat(struct spcl_inst* c, spcl_func_call f) {
    value sto;
    if (f.n_args < 2)
	return spcl_make_err(E_LACK_TOKENS, "cat() expected 2 arguments but got %lu", f.n_args);
    value l = f.args[0].v;
    value r = f.args[1].v;
    size_t l1 = l.n_els;
    size_t l2 = (r.type == VAL_LIST || r.type == VAL_ARRAY)? r.n_els : 1;
    //special case for matrices, just append a new row
    if (l.type == VAL_MAT && r.type == VAL_ARRAY) {
	sto.type = VAL_MAT;
	sto.n_els = l1 + 1;
	sto.val.l = (value*)malloc(sizeof(value)*sto.n_els);
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = spcl_copy_val(l.val.l[i]);
	sto.val.l[l1] = spcl_copy_val(r);
	return sto;
    }
    //otherwise we have to do something else
    if (l.type != VAL_LIST && r.type != VAL_ARRAY)
	return spcl_make_err(E_BAD_TYPE, "called cat() with types <%s> <%s>", valnames[l.type], valnames[r.type]);
    sto.type = l.type;
    sto.n_els = l1 + l2;
    //deep copy the first list/array
    if (sto.type == VAL_LIST) {
	sto.val.l = (value*)malloc(sizeof(value)*sto.n_els);
	if (!sto.val.l) return spcl_make_err(E_NOMEM, "");
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = spcl_copy_val(l.val.l[i]);
	if (r.type == VAL_LIST) {
	    //list -> list
	    for (size_t i = 0; i < l2; ++i)
		sto.val.l[i+l1] = spcl_copy_val(r.val.l[i]);
	} else if (r.type == VAL_ARRAY) {
	    //array -> list
	    for (size_t i = 0; i < l2; ++i)
		sto.val.l[i+l1] = spcl_make_num(r.val.a[i]);
	} else {
	    //anything -> list
	    sto.val.l[l1] = spcl_copy_val(r);
	}
    } else {
	sto.val.a = (double*)malloc(sizeof(double)*sto.n_els);
	if (!sto.val.a) return spcl_make_err(E_NOMEM, "");
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = spcl_copy_val(f.args[0].v.val.l[i]);
	if (r.type == VAL_LIST) {
	    //list -> array
	    for (size_t i = 0; i < l2; ++i) {
		if (r.val.l[i].type != VAL_NUM) {
		    free(sto.val.a);
		    return spcl_make_err(E_BAD_TYPE, "can only concatenate numeric lists to arrays");
		}
		sto.val.a[i+l1] = r.val.l[i].val.x;
	    }
	} else if (r.type == VAL_ARRAY) {
	    //array -> array
	    for (size_t i = 0; i < l2; ++i)
		sto.val.a[i+l1] = r.val.a[i];
	} else if (r.type == VAL_NUM) {
	    //number -> array
	    sto.val.a[l1] = r.val.x;
	} else {
	    return spcl_make_err(E_BAD_TYPE, "called cat() with types <%s> <%s>", valnames[l.type], valnames[r.type]);
	}
    }
    return sto;
}
/**
 * print the elements to the console
 */
value spcl_print(struct spcl_inst* c, spcl_func_call f) {
    value ret = spcl_make_none();
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].v.type == VAL_NUM) {
	    printf("%f", f.args[i].v.val.x);
	} else if (f.args[i].v.type == VAL_STR) {
	    printf("%s", f.args[i].v.val.s);
	} else {
	    printf("<object at %p>", f.args[i].v.val.l);
	}
    }
    printf("\n");
    return ret;
}
/**
 * Make a vector argument with the x,y, and z coordinates supplied
 */
static const valtype ARRAY_SIG[] = {VAL_LIST};
value spcl_array(spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, ARRAY_SIG);
    //treat matrices with one row as vectors
    if (f.n_args == 1) {
	if (f.args[0].v.val.l[0].type == VAL_LIST)
	    return spcl_cast(f.args[0].v, VAL_MAT);
	else
	    return spcl_cast(f.args[0].v, VAL_ARRAY);
    }
    value ret;
    //otherwise we need to do more work
    size_t n_cols = f.args[0].v.n_els;
    ret.type = VAL_MAT;
    ret.n_els = f.n_args;
    ret.val.l = malloc(sizeof(value)*f.n_args);
    //iterate through rows
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].v.type == VAL_LIST) { free(ret.val.l);return spcl_make_err(E_BAD_TYPE, "non list encountered in matrix"); }
	if (f.args[i].v.n_els != n_cols) { free(ret.val.l);return spcl_make_err(E_BAD_VALUE, "can't create matrix from ragged array"); }
	ret.val.l[i] = spcl_cast(f.args[i].v, VAL_ARRAY);
	//check for errors
	if (ret.val.l[i].type == VAL_ERR) {
	    free(ret.val.l);
	    ret = spcl_copy_val(ret.val.l[i]);
	    return ret;
	}
    }
    return ret;
}

value spcl_vec(spcl_inst* c, spcl_func_call f) {
    value ret = spcl_make_none();
    //just copy the elements
    ret.type = VAL_ARRAY;
    ret.n_els = f.n_args;
    //skip copying an empty list
    if (ret.n_els == 0)
	return ret;
    ret.val.a = malloc(sizeof(double)*ret.n_els);
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].v.type != VAL_NUM) {
	    free(ret.val.a);
	    return spcl_make_err(E_BAD_TYPE, "cannot cast list with non-numeric types to array");
	}
	ret.val.a[i] = f.args[i].v.val.x;
    }
    return ret;
}

//math functions
WRAP_MATH_FN(sin)
WRAP_MATH_FN(cos)
WRAP_MATH_FN(tan)
WRAP_MATH_FN(exp)
WRAP_MATH_FN(asin)
WRAP_MATH_FN(acos)
WRAP_MATH_FN(atan)
WRAP_MATH_FN(log)
WRAP_MATH_FN(sqrt)

/** ============================ struct value ============================ **/

value spcl_make_none() {
    value v;
    v.type = VAL_UNDEF;
    v.n_els = 0;
    v.val.x = 0;
    return v;
}

value spcl_make_err(parse_ercode code, const char* format, ...) {
    value ret;
    ret.type = VAL_ERR;
    //a nomemory error obviously won't be able to allocate any more memory
    if (code == E_NOMEM) {
	ret.val.e = NULL;
	return ret;
    }
    ret.val.e = malloc(sizeof(struct error));
    if (!ret.val.e)
	return ret;
    ret.val.e->c = code;
    va_list args;
    va_start(args, format);
    vsnprintf(ret.val.e->msg, ERR_BSIZE, format, args);
    va_end(args);
    return ret;
}

value spcl_make_num(double x) {
    value v;
    v.type = VAL_NUM;
    v.n_els = 1;
    v.val.x = x;
    return v;
}

value spcl_make_str(const char* s) {
    value v;
    v.type = VAL_STR;
    v.n_els = strlen(s) + 1;
    v.val.s = malloc(sizeof(char)*v.n_els);
    for (size_t i = 0; i < v.n_els; ++i) v.val.s[i] = s[i];
    return v;
}
value spcl_make_array(double* vs, size_t n) {
    value v;
    v.type = VAL_ARRAY;
    v.n_els = n;
    v.val.a = (double*)malloc(sizeof(double)*v.n_els);
    memcpy(v.val.a, vs, sizeof(double)*n);
    return v;
}
value spcl_make_list(const value* vs, size_t n_vs) {
    value v;
    v.type = VAL_LIST;
    v.n_els = n_vs;
    v.val.l = (value*)malloc(sizeof(value)*v.n_els);
    for (size_t i = 0; i < v.n_els; ++i) v.val.l[i] = spcl_copy_val(vs[i]);
    return v;
}
value spcl_make_fn(const char* name, size_t n_args, value (*p_exec)(spcl_inst*, spcl_func_call)) {
    value ret;
    ret.type = VAL_FUNC;
    ret.n_els = n_args;
    ret.val.f = make_spcl_uf_ex(p_exec);
    return ret;
}
value spcl_make_inst(spcl_inst* parent, const char* s) {
    value v;
    v.type = VAL_INST;
    v.val.c = make_spcl_inst(parent);
    if (s && s[0] != 0) {
	value tmp = spcl_make_str(s);
	spcl_set_value(v.val.c, "__type__", tmp, 0);
    }
    return v;
}
value spcl_valcmp(value a, value b) {
    if (a.type != b.type || a.type == VAL_ERR || b.type == VAL_ERR)
	return spcl_make_err(E_BAD_VALUE, "cannot compare types %s and %s", valnames[a.type], valnames[b.type]);
    if (a.type == VAL_NUM) {
	return spcl_make_num(a.val.x - b.val.x);
    } else if (a.type == VAL_STR) {
	//first make sure that both strings are not null while ensuring that null strings compare identically
	if (a.val.s == NULL) {
	    if (b.val.s)
		return spcl_make_num(1);
	    return spcl_make_num(0);
	}
	//we don't need to check whether a is null again, if it reached this point it is valid
	if (b.val.s == NULL)
	    return spcl_make_num(-1);
	return spcl_make_num(strcmp(a.val.s, b.val.s));
    } else if (a.type == VAL_LIST) {
	if (a.n_els != b.n_els)
	    return spcl_make_num(a.n_els - b.n_els);
	value tmp;
	for (size_t i = 0; i < a.n_els; ++i) {
	    tmp = spcl_valcmp(a.val.l[i], b.val.l[i]);
	    if (tmp.val.x)
		return tmp;
	}
	return spcl_make_num(0);
    } else if (a.type == VAL_ARRAY) {
	if (a.n_els != b.n_els)
	    return spcl_make_num(a.n_els - b.n_els);
	for (size_t i = 0; i < a.n_els; ++i) {
	    if (a.val.a[i] != b.val.a[i])
		return spcl_make_num(a.val.a[i] - b.val.a[i]);
	}
	return spcl_make_num(0);
    }
    return spcl_make_none();
}

int value_str_cmp(value a, const char* b) {
    if (a.type != VAL_STR)
	return 0;
    return strcmp(a.val.s, b);
}

char* spcl_stringify(value v, size_t* n) {
    //exit if there isn't enough space to write the null terminator
    if (v.type == VAL_STR) {
	if (n) *n = strlen(v.val.s);
	return strdup(v.val.s);
    } else if (v.type == VAL_ARRAY) {
	char* sto = (char*)malloc(sizeof(char)*BUF_SIZE);
	int off = 1;
	sto[0] = '{';//}
	for (size_t i = 0; i < v.n_els; ++i) {
	    size_t rem = BUF_SIZE-off;
	    if (rem > MAX_NUM_SIZE)
		rem = MAX_NUM_SIZE;
	    int tmp = write_numeric(sto+(size_t)off, rem, v.val.a[i]);
	    if (tmp < 0) {
		sto[off] = 0;
		return sto;
	    }
	    if (tmp >= rem) {
		sto[BUF_SIZE-1] = 0;
		if (n) *n = off-1;
		return sto;
	    }
	    off += (size_t)tmp;
	    if (i+1 < v.n_els)
		sto[off++] = ',';
	}
	if (off >= 0 && (size_t)off < BUF_SIZE)
	    sto[off++] = '}';
	sto[off] = 0;
	if (n) *n = off;
	return sto;
    } else if (v.type == VAL_NUM) {
	char* sto = (char*)malloc(sizeof(char)*MAX_NUM_SIZE);
	int tmp = write_numeric(sto, MAX_NUM_SIZE, v.val.x);
	if (n) *n = (size_t)tmp;
	return sto;
    } else if (v.type == VAL_LIST) {
	//TODO
    }
    return NULL;
}

value spcl_cast(value v, valtype t) {
    if (v.type == VAL_UNDEF)
	return spcl_make_err(E_BAD_TYPE, "cannot cast <undefined> to <%s>", valnames[t]);
    //trivial casts should just be copies
    if (v.type == t)
	return spcl_copy_val(v);
    value ret;
    ret.type = t;
    ret.n_els = v.n_els;
    if (t == VAL_LIST) {
	if (v.type == VAL_ARRAY) {
	    ret.val.l = (value*)malloc(sizeof(value)*ret.n_els);
	    for (size_t i = 0; i < ret.n_els; ++i)
		ret.val.l[i] = spcl_make_num(v.val.a[i]);
	    return ret;
	} else if (v.type == VAL_INST) {
	    //instance -> list
	    ret.n_els = v.val.c->n_memb;
	    ret.val.l = (value*)calloc(ret.n_els, sizeof(value));
	    if (!ret.val.l)
		return spcl_make_err(E_NOMEM, "");
	    for (size_t i = con_it_next(v.val.c, 0); i < con_size(v.val.c); i = con_it_next(v.val.c, i+1))
		ret.val.l[i] = spcl_copy_val(v.val.c->table[i].v);
	    ret.n_els = v.n_els;
	    return ret;
	} else if (v.type == VAL_MAT) {
	    //matrices are basically just an alias for lists
	    ret = spcl_copy_val(v);
	    ret.type = t;
	    return ret;
	}
    } else if (t == VAL_MAT) {
	if (v.type == VAL_LIST) {
	    ret.val.l = (value*)malloc(sizeof(value)*ret.n_els);
	    for (size_t i = 0; i < ret.n_els; ++i) {
		//first try making the element an array
		value tmp = spcl_cast(v.val.l[i], VAL_ARRAY);
		if (tmp.type == VAL_ERR) {
		    //if that doesn't work try making it a matrix
		    spcl_cleanup_val(&tmp);
		    tmp = spcl_cast(v.val.l[i], VAL_MAT);
		    if (tmp.type == VAL_ERR) {
			//if both of those failed, give up
			free(ret.val.l);
			return tmp;
		    }
		}
		ret.val.l[i] = tmp;
	    }
	    return ret;
	}
    } else if (t == VAL_ARRAY) {
	if (v.type == VAL_LIST) {
	    //list -> array
	    ret.n_els = v.n_els;
	    ret.val.a = (double*)malloc(sizeof(double)*ret.n_els);
	    if (!ret.val.a)
		return spcl_make_err(E_NOMEM, "");
	    for (size_t i = 0; i < ret.n_els; ++i) {
		if (v.val.l[i].type != VAL_NUM) {
		    free(ret.val.a);
		    return spcl_make_err(E_BAD_TYPE, "cannot cast list with non-numeric types to array");
		}
		ret.val.a[i] = v.val.l[i].val.x;
	    }
	    return ret;
	}
    } else if (t == VAL_STR) {
	//anything -> string
	ret.val.s = spcl_stringify(v, &(ret.n_els));
    }
    //if we reach this point in execution then there was an error
    ret.type = VAL_UNDEF;
    ret.n_els = 0;
    return ret;
}

void spcl_cleanup_val(value* v) {
    if (v->type == VAL_ERR) {
	free(v->val.e);
    } else if ((v->type == VAL_STR && v->val.s) || (v->type == VAL_ARRAY && v->val.a)) {
	free(v->val.s);
    } else if ((v->type == VAL_LIST || v->type == VAL_MAT) && v->val.l) {
	for (size_t i = 0; i < v->n_els; ++i)
	    spcl_cleanup_val(v->val.l + i);
	free(v->val.l);
    } else if (v->type == VAL_ARRAY && v->val.a) {
	free(v->val.a);
    } else if (v->type == VAL_INST && v->val.c) {
	destroy_spcl_inst(v->val.c);
    } else if (v->type == VAL_FUNC && v->val.f) {
	destroy_spcl_uf(v->val.f);
    }
    v->type = VAL_UNDEF;
    v->val.x = 0;
    v->n_els = 0;
}

value spcl_copy_val(const value o) {
    value ret;
    ret.type = o.type;
    ret.n_els = o.n_els;
    //strings or lists must be copied
    if (o.type == VAL_STR) {
	ret.val.s = (char*)malloc(sizeof(char)*o.n_els);
	for (size_t i = 0; i < o.n_els; ++i) ret.val.s[i] = o.val.s[i];
    } else if (o.type == VAL_ARRAY) {
	ret.val.a = (double*)malloc(sizeof(double)*o.n_els);
	for (size_t i = 0; i < o.n_els; ++i) ret.val.a[i] = o.val.a[i];
    } else if (o.type == VAL_LIST || o.type == VAL_MAT) {
	if (o.val.l == NULL) {
	    ret.n_els = 0;
	    ret.val.l = NULL;
	} else {
	    ret.val.l = (value*)calloc(o.n_els, sizeof(value));
	    for (size_t i = 0; i < o.n_els; ++i) ret.val.l[i] = spcl_copy_val(o.val.l[i]);
	}
    } else if (o.type == VAL_INST) {
	ret.val.c = copy_spcl_inst(o.val.c);
    } else if (o.type == VAL_FUNC) {
	ret.val.f = copy_spcl_uf(o.val.f);
    } else {
	ret.val.x = o.val.x;
    }
    return ret;
}

/**
 * swap the values stored at a and b
 */
void swap_val(value* a, value* b) {
    //swap type and number of elements
    valtype tmp = a->type;
    a->type = b->type;
    b->type = tmp;
    size_t tmp_n = a->n_els;
    a->n_els = b->n_els;
    b->n_els = tmp_n;
    union V tmp_v = a->val;
    a->val = b->val;
    b->val = tmp_v;
}

/**
 * handle an error at index i in a matrix
 * returns: whether there was an error
 */
static inline int matrix_err(value* l, size_t i) {
    if (i >= l->n_els || l->val.l[i].type == VAL_LIST || l->val.l[i].type == VAL_ARRAY)
	return 0;
    //handle incorrect types
    if (l->val.l[i].type != VAL_ERR) {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "matrix contains type <%s>", valnames[l->val.l[i].type]);
	return 1;
    }
    //move the error to overwrite the list
    value tmp = l->val.l[i];
    for (size_t j = 0; j < l->n_els; ++j) {
	if (i != j)
	    spcl_cleanup_val(l->val.l + j);
    }
    free(l->val.l);
    *l = tmp;
    return 1;
}

#if SPCL_DEBUG_LVL>0
static inline void print_spaces(FILE* f, size_t n) {
    for (size_t i = 0; i < n; ++i)
	fprintf(f, " |");
}
void print_hierarchy(value v, FILE* f, size_t depth) {
    if (f == NULL)
	f = stdout;
    //print the left tree view thingie
    print_spaces(f, depth);
    //now we handle all of the simple (non-recursive) prints
    if (v.type == VAL_UNDEF) {
	fprintf(f, "UNDEFINED\n");
    } else if (v.type == VAL_NUM) {
	fprintf(f, "%f\n", v.val.x);
    } else if (v.type == VAL_STR) {
	fprintf(f, "\"%s\"\n", v.val.s);
    } else if (v.type == VAL_FUNC) {
	fprintf(f, "%p\n", v.val.f);
    //now handle all the recursive prints
    } else if (v.type == VAL_LIST) {
	fprintf(f, "[+\n"/*]*/);
	for (size_t i = 0; i < v.n_els; ++i)
	    print_hierarchy(v.val.l[i], f, depth+1);
	print_spaces(f, depth);
	fprintf(f, /*[*/"]\n");
    } else if (v.type == VAL_ARRAY) {
	fprintf(f, "([+\n"/*])*/);
	for (size_t i = 0; i < v.n_els; ++i) {
	    print_spaces(f, depth+2);
	    fprintf(f, "|%f\n", v.val.a[i]);
	}
	print_spaces(f, depth+1);
	fprintf(f, /*([*/"])\n");
    } else if (v.type == VAL_INST) {
	fprintf(f, "{+\n"/*}*/);
	for (size_t i = con_it_next(v.val.c, 0); i < con_size(v.val.c); i = con_it_next(v.val.c, i+1)) {
	    name_val_pair pair = v.val.c->table[i];
	    print_spaces(f, depth+1);
	    fprintf(f, "%s:", pair.s);
	    //use space economically for simple types
	    if (pair.v.type == VAL_LIST || pair.v.type == VAL_ARRAY || pair.v.type == VAL_INST) {
		fprintf(f, " -V\n");
		print_hierarchy(pair.v, f, depth+1);
	    } else {
		fprintf(f, " ");
		print_hierarchy(pair.v, f, 0);
	    }
	}
	print_spaces(f, depth);
	fprintf(f, /*{*/"}\n");
    }
}
#endif

#if SPCL_DEBUG_LVL<1
static
#endif
void val_add(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = spcl_make_num( (l->val.x)+(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot add arrays of length %lu and %lu", l->n_els, r.n_els);
	} else {
	    for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] += r.val.a[i];
	}
    } else if (l->type == VAL_ARRAY && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i)
	    l->val.a[i] += r.val.x;
    } else if (l->type == VAL_MAT && r.type == VAL_MAT) {
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot add lists of length %lu and %lu", l->n_els, r.n_els);	
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_add(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l, i))
		return;
	}
    } else if (l->type == VAL_LIST) {
	l->n_els += 1;
	l->val.l = (value*)realloc(l->val.l, l->n_els);
	l->val.l[l->n_els-1] = spcl_copy_val(r);
    } else if (l->type == VAL_STR) {
	size_t l_len, r_len;
	l_len = l->n_els;
	char* r_str = spcl_stringify(r, &r_len);
	//create a new string and copy
	char* n_str = (char*)malloc(sizeof(char)*(l_len+r_len));//l_len already includes null terminator so we have enough
	char* tmp = stpncpy(n_str, l->val.s, l_len);
	tmp = stpncpy(tmp, r_str, r_len);
	tmp[0] = 0;
	free(r_str);
	//now set the value
	l->n_els = l_len+r_len;
	free(l->val.s);//free the old string
	l->val.s = n_str;
    } else {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "cannot add types %s and %s", valnames[l->type], valnames[r.type]);
    }
}

#if SPCL_DEBUG_LVL<1
static
#endif
void val_sub(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = spcl_make_num( (l->val.x)-(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot subtract arrays of length %lu and %lu", l->n_els, r.n_els);
	} else {
	    for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] -= r.val.a[i];
	}
    } else if (l->type == VAL_ARRAY && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i)
	    l->val.a[i] -= r.val.x;
    } else if (l->type == VAL_MAT && r.type == VAL_MAT) {
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot subtract lists of length %lu and %lu", l->n_els, r.n_els);	
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_sub(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l, i))
		return;
	}
    } else {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "cannot subtract types %s and %s", valnames[l->type], valnames[r.type]);
    }
}

#if SPCL_DEBUG_LVL<1
static
#endif
void val_mul(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = spcl_make_num( (l->val.x)*(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot multiply arrays of length %lu and %lu", l->n_els, r.n_els);
	    return;
	} else {
	    for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] *= r.val.a[i];
	}
    } else if (l->type == VAL_ARRAY && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i)
	    l->val.a[i] *= r.val.x;
    } else if (l->type == VAL_MAT && r.type == VAL_MAT) {
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot multiply lists of length %lu and %lu", l->n_els, r.n_els);	
	    return;
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_mul(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l,i))
		return;
	}
    } else if (l->type == VAL_MAT && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_mul(l->val.l+i, r);
	    if (matrix_err(l, i))
		return;
	}
    } else {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "cannot multiply types %s and %s", valnames[l->type], valnames[r.type]);
    }
}

#if SPCL_DEBUG_LVL<1
static
#endif
void val_div(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = spcl_make_num( (l->val.x)/(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot divide arrays of length %lu and %lu", l->n_els, r.n_els);
	    return;
	} else {
	    for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] /= r.val.a[i];
	}
    } else if (l->type == VAL_ARRAY && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i)
	    l->val.a[i] /= r.val.x;
    } else if (l->type == VAL_MAT && r.type == VAL_MAT) {
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot divide lists of length %lu and %lu", l->n_els, r.n_els);	
	    return;
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_div(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l,i))
		return;
	}
    } else if (l->type == VAL_MAT && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_div(l->val.l+i, r);
	    if (matrix_err(l, i))
		return;
	}
    } else {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "cannot divide types %s and %s", valnames[l->type], valnames[r.type]);
    }
}

#if SPCL_DEBUG_LVL<1
static
#endif
void val_exp(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = spcl_make_num( pow(l->val.x, r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot raise arrays of length %lu and %lu", l->n_els, r.n_els);
	    return;
	} else {
	    for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] = pow(l->val.a[i], r.val.a[i]);
	}
    } else if (l->type == VAL_ARRAY && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i)
		l->val.a[i] = pow(l->val.a[i], r.val.x);
    } else if (l->type == VAL_MAT && r.type == VAL_MAT) {
	if (l->n_els != r.n_els) {
	    spcl_cleanup_val(l);
	    *l = spcl_make_err(E_OUT_OF_RANGE, "cannot raise matrices of length %lu and %lu", l->n_els, r.n_els);	
	    return;
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_exp(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l,i))
		return;
	}
    } else if (l->type == VAL_MAT && r.type == VAL_NUM) {
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_exp(l->val.l+i, r);
	    if (matrix_err(l, i))
		return;
	}
    } else {
	spcl_cleanup_val(l);
	*l = spcl_make_err(E_BAD_TYPE, "cannot raise types %s and %s", valnames[l->type], valnames[r.type]);
    }
}

/** ============================ spcl_inst ============================ **/

//helper to convert possibly negative index values to real C indices
static inline size_t index_to_abs(value* ind, size_t max_n) {
    if (-(ind->val.x) > max_n || ind->val.x >= max_n) {
	*ind = spcl_make_err(E_OUT_OF_RANGE, "index %d out of bounds for list of size %lu", (int)ind->val.x, max_n);
	return 0;
    }
    if (ind->val.x < 0)
	return max_n - (size_t)(-ind->val.x);
    return (size_t)(ind->val.x);
}

//non-cryptographically hash the string str reading only the first n bytes
static inline size_t fnv_1(const char* str, size_t n, unsigned char t_bits) {
    if (str == NULL)
	return 0;
    size_t ret = FNV_OFFSET;
    for (size_t i = 0; str[i] && i < n; ++i) {
	if (!is_whitespace(str[i])) {
	    ret = ret^str[i];
	    ret = ret*FNV_PRIME;
	}
    }
    return ((ret >> t_bits) ^ ret) % (1 << t_bits);
    /**TODO: try using this if the above gives poor dispersion
#if TABLE_BITS > 15
    return (ret >> t_bits) ^ (ret & TABLE_MASK(t_bits));
#else
    return ((ret >> t_bits) ^ ret) & TABLE_MASK(t_bits);
#endif
*/
}

/**
 * Get the index i that contains the string name
 * c: the spcl_inst to look in
 * name: the name to look for
 * ind: the location where we find the matching index
 * returns: 1 if a match was found, 0 otherwise
 */
static inline int find_ind(const struct spcl_inst* c, const char* name, size_t n, size_t* ind) {
    size_t ii = fnv_1(name, n, c->t_bits);
    size_t i = ii;
    while (c->table[i].s) {
	if (namecmp(name, c->table[i].s, n) == 0) {
	    *ind = i;
	    return 1;
	}
	//i = (i+1) if i+1 < table_size or 0 otherwise (note table_size == con_size(c))
	i = (i+1) & (con_size(c)-1);
	if (i == ii)
	    break;
    }
    *ind = i;
    return 0;
}

/**
 * Grow the spcl_inst if necessary
 * returns: 1 if growth was performed
 */
static inline int grow_inst(struct spcl_inst* c) {
    if (c && c->n_memb*GROW_LOAD_DEN > con_size(c)*GROW_LOAD_NUM) {
	//create a new spcl_inst with twice as many elements
	struct spcl_inst nc;
	nc.parent = c->parent;
	nc.t_bits = c->t_bits + 1;
	nc.n_memb = 0;
	nc.table = (name_val_pair*)calloc(con_size(&nc), sizeof(name_val_pair));
	//make sure allocation was successful
	if (!nc.table) {
	    printf("Fatal error: ran out of memory!\n");
	    exit(1);
	}
	//we have to rehash every member in the old table
	for (size_t i = 0; i < con_size(c); ++i) {
	    if (c->table[i].s == NULL)
		continue;
	    //only move non-null members
	    size_t new_ind;
	    if (!find_ind(&nc, c->table[i].s, SIZE_MAX, &new_ind))
		++nc.n_memb;
	    nc.table[new_ind] = c->table[i];
	}
	//deallocate old table and replace it with the new one
	free(c->table);
	*c = nc;
	return 1;
    }
    return 0;
}
/**
 * include builtin functions
 * TODO: make this not dumb
 */
static inline void setup_builtins(struct spcl_inst* c) {
    //create builtins
    spcl_set_value(c, "false",	spcl_make_num(0), 0);
    spcl_set_value(c, "true",	spcl_make_num(1), 0);//create horrible (if amusing bugs when someone tries to assign to true or false
    spcl_add_fn(c, spcl_assert,		"assert");
    spcl_add_fn(c, spcl_typeof,		"typeof");
    spcl_add_fn(c, spcl_len,		"len");
    spcl_add_fn(c, spcl_range,		"range");
    spcl_add_fn(c, spcl_linspace,	"linspace");
    spcl_add_fn(c, spcl_flatten,	"flatten");
    spcl_add_fn(c, spcl_array,		"array");
    spcl_add_fn(c, spcl_vec,		"vec");
    spcl_add_fn(c, spcl_cat,		"cat");
    spcl_add_fn(c, spcl_print,		"print");
    //math stuff
    value tmp = spcl_make_inst(c, "math");
    spcl_inst* math_c = tmp.val.c;
    spcl_set_value(math_c, "pi", 	spcl_make_num(M_PI), 0);
    spcl_set_value(math_c, "e", 	spcl_make_num(M_E), 0);
    spcl_add_fn(math_c, spcl_sin,	"sin");
    spcl_add_fn(math_c, spcl_cos,	"cos");
    spcl_add_fn(math_c, spcl_tan,	"tan");
    spcl_add_fn(math_c, spcl_exp,	"exp");
    spcl_add_fn(math_c, spcl_asin,	"asin");
    spcl_add_fn(math_c, spcl_acos,	"acos");
    spcl_add_fn(math_c, spcl_atan,	"atan");
    spcl_add_fn(math_c, spcl_log,	"log");
    spcl_add_fn(math_c, spcl_sqrt,	"sqrt");
    spcl_set_value(c, "math", tmp, 0);
}

struct spcl_inst* make_spcl_inst(spcl_inst* parent) {
    spcl_inst* c = (spcl_inst*)malloc(sizeof(spcl_inst));
    if (!c)
	return NULL;
    c->parent = parent;
    c->n_memb = 0;
    c->t_bits = DEF_TAB_BITS;
    //double the allocated size for root insts (since they're likely to hold more stuff)
    if (!parent) c->t_bits++;
    c->table = (name_val_pair*)calloc(con_size(c), sizeof(name_val_pair));
    if (!c->table) {
	free(c);
	return NULL;
    }
    if (!parent) {
	setup_builtins(c);
    }
    return c;
}

struct spcl_inst* copy_spcl_inst(const spcl_inst* o) {
    if (!o)
	return NULL;
    spcl_inst* c = (spcl_inst*)malloc(sizeof(spcl_inst));
    if (!c)
	return NULL;
    c->table = (name_val_pair*)calloc(con_size(o), sizeof(name_val_pair));
    if (!c->table) {
	free(c);
	return NULL;
    }
    c->parent = o->parent;
    c->n_memb = o->n_memb;
    c->t_bits = o->t_bits;
    for (size_t i = con_it_next(o, 0); i < con_size(o); i = con_it_next(o, i+1)) {
	c->table[i].s = strdup(o->table[i].s);
	c->table[i].v = spcl_copy_val(o->table[i].v);
    }
    return c;
}

void destroy_spcl_inst(struct spcl_inst* c) {
    if (!c)
	return;
    //erase the hash table
    for (size_t i = con_it_next(c, 0); i < con_size(c); i = con_it_next(c,i+1))
	cleanup_name_val_pair(c->table[i]);
    free(c->table);
    free(c);
}
/**
 * Get the location of the first operator which is not enclosed in a block expression
 * op_loc: store the location of the operator
 * open_ind: store the location of the first enclosing block
 * close_ind: store the location of the last escaping block
 * returns: the index of the first expression or 0 in the event of an error (it is impossible for a valid expression to have an operator as the first character)
 */
static inline value find_operator(read_state rs, lbi* op_loc, lbi* open_ind, lbi* close_ind, lbi* new_end) {
    *op_loc = rs.end;
    *open_ind = rs.end;*close_ind = rs.end;
    //keeps track of open and close [], (), {}, and ""
    stack(char) blk_stk = make_stack(char)();
    //variable names are not allowed to start with '+', '-', or a digit and may not contain any '.' symbols. Use this to check whether the value is numeric
    char open_type, prev;
    char cur = 0;

    //keep track of the precedence of the orders of operation (lower means executed later) ">,=,>=,==,<=,<"=4 "+,-"=3, "*,/"=2, "**"=1
    char op_prec = 0;
    for (; lbicmp(rs.start, rs.end) < 0 || blk_stk.ptr; rs.start = lb_add(rs.b, rs.start, 1)) {
	//make sure we don't read past the end of the file
	if (lbicmp(rs.start, lb_end(rs.b)) >= 0)
	    break;
	prev = cur;
	cur = lb_get(rs.b, rs.start);
	char next = lb_get(rs.b, lb_add(rs.b, rs.start, 1));
	if (cur == '(' || cur == '{' || cur == '[') {
	    push(char)(&blk_stk, cur);
	    //only set the open index if this is the first match
	    if (lbicmp(*open_ind, rs.end) == 0) *open_ind = rs.start;
	} else if (cur == ']' || cur == ')' || cur == '}') {
	    if (pop(char)(&blk_stk, &open_type) || cur != get_match(open_type)) {
		destroy_stack(char)(&blk_stk, NULL);
		return spcl_make_err(E_BAD_SYNTAX, "unexpected %c", cur);
	    }
	    *close_ind = rs.start;
	} else if (cur == '\"' && (prev != '\\')) {
	    if (peek(char)(&blk_stk, 1, &open_type) || cur != get_match(open_type)) {
		//quotes need to be handled in a special way
		push(char)(&blk_stk, cur);
		//only set the open index if this is the first match
		if (lbicmp(*open_ind, rs.end) == 0) *open_ind = rs.start;
	    } else {
		pop(char)(&blk_stk, &open_type);
		*close_ind = rs.start;
	    }
	} else if (cur == '/') {
	    //ignore everything after a comment
	    if (next == '/') {
		rs.start.off = rs.b->line_sizes[rs.start.line];
		break;
	    }
	}

	if (blk_stk.ptr == 0) {
	    //check for operations with higher priorities (higher priorities indicated by op_prec are executed later in order of operations)
	    if (cur == '=' && next != '=' && op_prec < 7) {
		op_prec = 7;
		*op_loc = rs.start;
	    } else if (((cur == '=' && next == '=') || cur == '>' || cur == '<') && op_prec < 6) {
		op_prec = 6;
		*op_loc = rs.start;
		//if this is a two character sequence like ==, >=, or <= we need to skip the next sign
		if (next == '=')
		    rs.start = lb_add(rs.b, rs.start, 1);
	    } else if ((cur == '!' || (cur == '&' && next == '&') || (cur == '|' && next == '|')) && op_prec < 5) {
		op_prec = 5;
		*op_loc = rs.start;
		//if this is a two character sequence like ==, >=, or <= we need to skip the next sign
		if (next == '=')
		    rs.start = lb_add(rs.b, rs.start, 1);
	    } else if ((cur == '+' || cur == '-') && prev != 'e' && prev != 0 && op_prec < 4) {
		//remember to recurse after we finish looping
		op_prec = 4;
		*op_loc = rs.start;
	    } else if (cur == '^' && op_prec < 3) {
		op_prec = 3;
		*op_loc = rs.start;
	    } else if ((cur == '*' || cur == '/') && op_prec < 2) {
		op_prec = 2;
		*op_loc = rs.start;
	    } else if (op_prec < 1 && cur == '?') {
		op_prec = 1;
		*op_loc = rs.start;
	    } else if (cur == ';') {
		break;
	    }
	}
    }
    if (blk_stk.ptr > 0) {
	pop(char)(&blk_stk, &open_type);
	destroy_stack(char)(&blk_stk, NULL);
	return spcl_make_err(E_BAD_SYNTAX, "expected %c", get_match(open_type));
    }
    if (new_end) *new_end = rs.start;
    destroy_stack(char)(&blk_stk, NULL);
    return spcl_make_none();
}
//forward declare so that helpers can call
static inline value spcl_parse_line_rs(spcl_inst* c, read_state rs, lbi* new_end);
/**
 * An alternative to lookup which only considers the first n bytes in str
 */
static inline value spcl_find_rs(spcl_inst* c, read_state rs) {
    lbi dot_loc = strchr_block_rs(rs.b, rs.start, rs.end, '.');
    lbi ref_loc = strchr_block_rs(rs.b, rs.start, rs.end, '[');//]
    if (!lbicmp(dot_loc, rs.end) && !lbicmp(ref_loc, rs.end)) {
	//if there was neither a period or open brace, just lookup directly
	size_t i, n;
	//TODO: eliminate memory allocation and freeing since its slow
	char* str = trim_whitespace(lb_get_line(rs.b, rs.start, rs.end, NULL), &n);
	while (c) {
	    if (find_ind(c, str, n, &i)) {
		free(str);
		return c->table[i].v;
	    }
	    //go up if we didn't find it
	    c = c->parent;
	}
	free(str);
	//reaching this point in execution means the matching entry wasn't found
	return spcl_make_none();
    } else if (lbicmp(dot_loc, ref_loc) < 0) {
	//if there was a dot, access spcl_inst members
	value sub_con = spcl_find_rs(c, make_read_state(rs.b, rs.start, dot_loc));
	if (sub_con.type != VAL_INST)
	    return spcl_make_err(E_BAD_TYPE, "cannot access member from non instance type %s", valnames[sub_con.type]);
	return spcl_find_rs(sub_con.val.c, make_read_state(rs.b, lb_add(rs.b,dot_loc,1), rs.end));
    } else {
	//access lists/arrays
	lbi close_ind = strchr_block_rs(rs.b, lb_add(rs.b, ref_loc, 1), rs.end, /*[*/']');
	if (lbicmp(close_ind, rs.end) > 0)
	    return spcl_make_err(E_BAD_SYNTAX, /*[*/"expected ']'");
	//read the index
	value index = spcl_parse_line_rs(c, make_read_state(rs.b, lb_add(rs.b, ref_loc, 1), close_ind), NULL);
	if (index.type == VAL_ERR)
	    return index;
	if (index.type != VAL_NUM)
	    return spcl_make_err(E_BAD_TYPE, "cannot index list with type %s", valnames[index.type]);
	//read the list and convert to an absolute index
	value lst = spcl_find_rs(c, make_read_state(rs.b, rs.start, ref_loc));
	size_t i = index_to_abs(&index, lst.n_els);
	//branch based on type
	if (lst.type == VAL_LIST || lst.type == VAL_MAT) {
	    //the user might want to access a variable in a list of objects, handle that case
	    if (lbicmp(dot_loc, rs.end) < 0) {
		if (lst.val.l[i].type != VAL_INST)
		    return spcl_make_err(E_BAD_TYPE, "cannot access member from non instance type %s", valnames[lst.val.l[i].type]);
		return spcl_find_rs(lst.val.l[i].val.c, make_read_state(rs.b, lb_add(rs.b, close_ind, 1), rs.end));
	    }
	    return lst.val.l[i];
	} else if (lst.type == VAL_ARRAY) {
	    return spcl_make_num(lst.val.a[i]);
	} else {
	    return spcl_make_err(E_BAD_TYPE, "cannot index type %s", valnames[lst.type]);
	}
    }
}
/**
 * similar to set_value(), but read in place from a read state
 */
static inline value set_value_rs(struct spcl_inst* c, read_state rs, value p_val) {
    lbi dot_loc = strchr_block_rs(rs.b, rs.start, rs.end, '.');
    lbi ref_loc = strchr_block_rs(rs.b, rs.start, rs.end, '[');//]
    //if there are no dereferences, just access the table directly
    if (!lbicmp(dot_loc, rs.end) && !lbicmp(ref_loc, rs.end)) {
	char* str = trim_whitespace(lb_get_line(rs.b, rs.start, rs.end, NULL), NULL);
	spcl_set_value(c, str, p_val, 0);
	free(str);
	return spcl_make_none();
    } else if (lbicmp(dot_loc, ref_loc) < 0) {
	//access spcl_inst members
	value sub_con = spcl_find_rs(c, make_read_state(rs.b, rs.start, dot_loc));
	if (sub_con.type != VAL_INST)
	    return spcl_make_err(E_BAD_TYPE, "cannot access member from non instance type %s", valnames[sub_con.type]);
	return set_value_rs(sub_con.val.c, make_read_state(rs.b, lb_add(rs.b, dot_loc, 1), rs.end), p_val);
    } else {
	//access lists/arrays
	lbi close_ind = strchr_block_rs(rs.b, lb_add(rs.b, ref_loc, 1), rs.end, /*[*/']');
	if (lbicmp(close_ind, rs.end) > 0) return spcl_make_err(E_BAD_SYNTAX, /*[*/"expected ']'");
	//read the index
	value index = spcl_parse_line_rs(c, make_read_state(rs.b, lb_add(rs.b, ref_loc, 1), close_ind), NULL);
	if (index.type == VAL_ERR) return index;
	if (index.type != VAL_NUM) return spcl_make_err(E_BAD_TYPE, "cannot index list with type %s", valnames[index.type]);
	//read the list and convert to an absolute index
	value lst = spcl_find_rs(c, make_read_state(rs.b, rs.start, ref_loc));
	size_t i = index_to_abs(&index, lst.n_els);
	//branch based on type
	if (lst.type == VAL_LIST || lst.type == VAL_MAT) {
	    //the user might want to access a variable in a list of objects, handle that case
	    if (lbicmp(dot_loc, rs.end) < 0) {
		if (lst.val.l[i].type != VAL_INST)
		    return spcl_make_err(E_BAD_TYPE, "cannot access member from non instance type %s", valnames[lst.val.l[i].type]);
		return set_value_rs(lst.val.l[i].val.c, make_read_state(rs.b, lb_add(rs.b, close_ind, 1), rs.end), p_val);
	    }
	    lst.val.l[i] = p_val;
	} else if (lst.type == VAL_ARRAY) {
	    if (p_val.type != VAL_NUM)
		return spcl_make_err(E_BAD_TYPE, "cannot assign type %s to array", valnames[p_val.type]);
	    lst.val.a[i] = p_val.val.x;
	} else {
	    return spcl_make_err(E_BAD_TYPE, "cannot index type %s", valnames[lst.type]);
	}
    }
    return spcl_make_none();
}
//TODO: to inline or not to inline
#if SPCL_DEBUG_LVL<1
static
#endif
value do_op(spcl_inst* c, read_state rs, lbi op_loc) {
    value sto = spcl_make_none();
    //some operators (==, >=, <=) take up more than one character, test for these
    char op = lb_get(rs.b, op_loc);
    char next = lb_get(rs.b, lb_add(rs.b, op_loc, 1));
    int op_width = 1;
    if (op != '?' && op != '.' && op != '|' && op != '&' && next == '=')
	op_width = 2;
    else if ((op == '|' || op == '&') && (next == op))
	op_width = 2;
    //set a read state before the operator and after the operator
    read_state rs_l = rs;
    read_state rs_r = rs;
    rs_l.end = op_loc;
    rs_r.start = lb_add(rs.b, op_loc, op_width);
    if (op == '?') {
	//ternary operators and dereferences are special cases
	//the colon must be present
	lbi col_loc = strchr_block_rs(rs.b, op_loc, rs.end, ':');
	if (lbicmp(col_loc, rs.end) >= 0)
	    return spcl_make_err(E_BAD_SYNTAX, "expected ':' in ternary");

	value l = spcl_parse_line_rs(c, rs_l, NULL);
	if (l.type == VAL_ERR)
	    return l;
	//0 branch
	if (l.type == VAL_UNDEF || l.val.x == 0) {
	    rs_r.start = lb_add(rs.b, col_loc, 1);
	    sto = spcl_parse_line_rs(c, rs_r, NULL);
	    return sto;
	} else {
	    //1 branch
	    rs_r.end = col_loc;
	    sto = spcl_parse_line_rs(c, rs_r, NULL);
	    return sto;
	}
    } else if (op == '=' && op_width == 1) {
	//assignments
	value tmp_val = spcl_parse_line_rs(c, rs_r, NULL);
	if (tmp_val.type == VAL_ERR)
	    return tmp_val;
	set_value_rs(c, rs_l, tmp_val);
	return spcl_make_none();
    } else if (op == '!' && op_width == 1) {
	value tmp_val = spcl_parse_line_rs(c, rs_r, NULL);
	if (tmp_val.type == VAL_ERR)
	    return tmp_val;
	if (tmp_val.type == VAL_UNDEF || (tmp_val.type == VAL_NUM && tmp_val.val.x == 0))
	    return spcl_make_num(1);
	return spcl_make_num(0);
    }

    //parse right and left values
    value l = spcl_parse_line_rs(c, rs_l, NULL);
    if (l.type == VAL_ERR)
	return l;
    value r = spcl_parse_line_rs(c, rs_r, NULL);
    if (r.type == VAL_ERR) {
	spcl_cleanup_val(&l);
	return r;
    }
    //WLOG fix all array operations to have the array first
    if (l.type == VAL_NUM && r.type == VAL_ARRAY)
	swap_val(&l, &r);
    sto.type = VAL_NUM;
    sto.n_els = 1;
    //handle equality comparisons
    if (op == '!' || op == '=' || op == '>' || op == '<') {
	value cmp = spcl_valcmp(l,r);
	spcl_cleanup_val(&l);
	spcl_cleanup_val(&r);
	if (cmp.type == VAL_ERR)
	    return cmp;
	if (op == '=')
	    return spcl_make_num(!cmp.val.x);
	if (op == '!')
	    return cmp;
	if (op == '>') {
	    if (op_width == 2)
		return spcl_make_num(cmp.val.x >= 0);
	    return spcl_make_num(cmp.val.x > 0);
	} else {
	    if (op_width == 2)
		return spcl_make_num(cmp.val.x <= 0);
	    return spcl_make_num(cmp.val.x < 0);
	}
    } else if (op == '|' || op == '&') {
	if (next != op)
	    return spcl_make_err(E_BAD_SYNTAX, "invalid operation \'%c%c\'", op, next);
	if (l.type == VAL_NUM && r.type == VAL_NUM)
	    return (op == '|')? spcl_make_num(l.val.x || r.val.x) : spcl_make_num(l.val.x && r.val.x);
	//undefined == false
	if (l.type == VAL_UNDEF)
	    return (op == '|')? spcl_make_num(r.val.x) : spcl_make_num(0);
	if (r.type == VAL_UNDEF)
	    return (op == '|')? spcl_make_num(1) : spcl_make_num(0);
	return spcl_make_num(1);
    } else {
	//arithmetic is all relatively simple
	switch(op) {
	case '+': val_add(&l, r);break;
	case '-': val_sub(&l, r);break;
	case '*': if (next == '*') { val_exp(&l, r); } else { val_mul(&l, r); }break;
	case '/': val_div(&l, r);break;
	}
	//if this is a relative assignment, do that
	if (next == '=') {
	    char* str = trim_whitespace(lb_get_line(rs.b, rs_l.start, rs_l.end, NULL), NULL);
	    spcl_set_value(c, str, l, 0);
	    free(str);
	    l = spcl_make_none();
	}
	spcl_cleanup_val(&r);
	return l;
    }
}
struct for_state {
    char* var_name;
    read_state expr_name;
    lbi for_start;
    lbi in_start;
    value it_list;
    name_val_pair prev;
    size_t var_ind;
};
static inline struct for_state make_for_state(spcl_inst* c, read_state rs, lbi for_start, value* er) {
    struct for_state fs;
    lbi after_for = lb_add(rs.b, for_start, strlen("for"));
    //now look for a block labeled "in"
    fs.for_start = for_start;
    fs.in_start = token_block(rs.b, after_for, rs.end, "in", strlen("in"));
    if (!lbicmp(fs.in_start, rs.end)) {
	*er = spcl_make_err(E_BAD_SYNTAX, "expected keyword in");
	return fs;
    }
    //the variable name is whatever is in between the "for" and the "in"
    while (is_whitespace(lb_get(rs.b, after_for)))
	after_for = lb_add(rs.b, after_for, 1);
    fs.var_name = trim_whitespace(lb_get_line(rs.b, after_for, fs.in_start, NULL), NULL);
    //now parse the list we iterate over
    lbi after_in = lb_add(rs.b, fs.in_start, strlen("in"));
    fs.it_list = spcl_parse_line_rs(c, make_read_state(rs.b, after_in, rs.end), NULL);
    if (fs.it_list.type == VAL_ERR) {
	*er = spcl_make_err(E_BAD_SYNTAX, "in expression %s", rs.b->lines[after_in.line]+after_in.off);
	free(fs.var_name);
	return fs;
    }
    if (fs.it_list.type != VAL_ARRAY && fs.it_list.type != VAL_LIST) {
	*er =  spcl_make_err(E_BAD_TYPE, "can't iterate over type %s", valnames[fs.it_list.type]);
	free(fs.var_name);
	return fs;
    }
    fs.expr_name = make_read_state(rs.b, lb_add(rs.b, rs.start, 1), fs.for_start);
    //we need to add a variable with the appropriate name to loop over. We write a value and save the value there before so we can remove it when we're done
    find_ind(c, fs.var_name, SIZE_MAX, &(fs.var_ind));
    fs.prev = c->table[fs.var_ind];
    c->table[fs.var_ind].s = fs.var_name;
    *er = spcl_make_none();
    return fs;
}
static inline void finish_for_state(spcl_inst* c, struct for_state fs) {
    //we need to reset the table with the loop index before iteration
    cleanup_name_val_pair(c->table[fs.var_ind]);
    c->table[fs.var_ind] = fs.prev;
    //free the memory from the iteration list
    spcl_cleanup_val(&fs.it_list);
}
//helper for spcl_parse_line to hand list literals
static inline value parse_literal_list(struct spcl_inst* c, read_state rs, lbi open_ind, lbi close_ind) {
    rs.end = close_ind;
    value sto;
    //read the coordinates separated by spaces
    value* lbuf;
    //check if this is a list interpretation
    lbi for_start = token_block(rs.b, lb_add(rs.b, open_ind, 1), close_ind, "for", strlen("for"));
    if (lbicmp(for_start, close_ind) < 0) {
	struct for_state fs = make_for_state(c, rs, for_start, &sto);
	if (sto.type == VAL_ERR)
	    return sto;
	//setup a buffer to hold the list
	sto.n_els = fs.it_list.n_els;
	lbuf = calloc(sto.n_els, sizeof(value));
	//we now iterate through the list specified, substituting VAL in the expression with the current value
	for (size_t i = 0; i < sto.n_els; ++i) {
	    if (fs.it_list.type == VAL_LIST)
		c->table[fs.var_ind].v = fs.it_list.val.l[i];
	    else if (fs.it_list.type == VAL_ARRAY)
		c->table[fs.var_ind].v = spcl_make_num(fs.it_list.val.a[i]);
	    lbuf[i] = spcl_parse_line_rs(c, fs.expr_name, NULL);
	    if (lbuf[i].type == VAL_ERR) {
		value ret = spcl_copy_val(lbuf[i]);
		for (size_t j = 0; j < i; ++j)
		    spcl_cleanup_val(lbuf+j);
		free(lbuf);
		finish_for_state(c, fs);
		return ret;
	    }
	}
	finish_for_state(c, fs);
    } else {
	//TODO: figure out how to avoid code duplication with parse_literal_func
	size_t alloc_n = ALLOC_LST_N;
	lbuf = malloc(sizeof(value)*alloc_n);
	sto.n_els = 0;
	lbi s = open_ind;
	lbi e = open_ind;
	while (lbicmp(s, close_ind) < 0 && lbicmp(e, close_ind) < 0) {
	    s = lb_add(rs.b, e, 1);
	    e = strchr_block_rs(rs.b, s, close_ind, ',');
	    if (sto.n_els == alloc_n) {
		alloc_n *= 2;
		lbuf = realloc(lbuf, sizeof(value)*alloc_n);
		if (!lbuf)
		    return spcl_make_err(E_NOMEM, "");
	    }
	    lbuf[sto.n_els] = spcl_parse_line_rs(c, make_read_state(rs.b, s, e), NULL);
	    if (lbuf[sto.n_els].type == VAL_ERR) {
		sto = spcl_copy_val(lbuf[sto.n_els]);
		free(lbuf);
		return sto;
	    }
	    //only include defined values
	    if (lbuf[sto.n_els].type != VAL_UNDEF)
		sto.n_els++;
	}
    }
    //set number of elements and type
    sto.type = VAL_LIST;
    sto.val.l = lbuf;
    return sto;
}

/**
 * Given a read state, read a list of each occurrence of a comma separator between open_ind and close_ind.
 * returns: a list of the location of each comma and the open and close brace. If the returned value is called args, then the characters between (args[i], args[i+1]) (non-inclusive) give the ith string
 */
static inline lbi* csv_to_inds(const spcl_line_buffer* lb, lbi open_ind, lbi close_ind, size_t* n_inds) {
    //get a list of each argument index plus an additional token at the end.
    size_t alloc_n = ALLOC_LST_N;
    lbi* inds = malloc(sizeof(lbi)*alloc_n);
    size_t i = 0;
    lbi e = open_ind;
    lbi s = e;
    while (lbicmp(s, close_ind) < 0) {
	s = e;
	e = strchr_block_rs(lb, lb_add(lb, s, 1), close_ind, ',');
	if (i+1 == alloc_n) {
	    alloc_n *= 2;
	    inds = realloc(inds, sizeof(lbi)*alloc_n);
	    if (!inds)
		return NULL;
	}
	inds[i++] = s;
    }
    if (i == 0) {
	free(inds);
	*n_inds = 0;
	return NULL;
    }
    *n_inds = i-1;
    return inds;
}

/**
 * Read the name value pairs separated by an '=' character from the strings in inds to sto. The caller must ensure that *n is the length of inds and sto has enough allocated memory to hold *n-1 elements. Save the number of elements written to n
 */
//TODO: Since there are a lot of static optimizations possible dependent on assign_by_name this may or may not be fater inlined. Figure that out.
static inline value inds_to_nvp(spcl_inst* c, const spcl_line_buffer* lb, name_val_pair* sto, lbi* inds, size_t* n, int assign_by_name) {
    if (!n)
	return spcl_make_err(E_BAD_VALUE, "tried to read NULL");
    size_t n_init = *n;
    //we setup inds such that consecutive indices give strings to read
    size_t j = 0;
    lbi eq_loc;
    for (size_t i = 0; i < n_init; ++i) {
	if (assign_by_name) {
	    eq_loc = strchr_block_rs(lb, lb_add(lb, inds[i], 1), inds[i+1], '=');
	    char prev = lb_get(lb, lb_sub(lb, eq_loc, 1));
	    char next = lb_get(lb, lb_add(lb, eq_loc, 1));
	    if (next == '=' || prev == '<' || prev == '>')
		eq_loc = inds[i+1];
	}
	//if there is an '=' sign read the name
	if (assign_by_name && lbicmp(eq_loc, inds[i+1]) < 0) {
	    sto[j].s = trim_whitespace(lb_get_line(lb, lb_add(lb, inds[i], 1), eq_loc, NULL), NULL);
	    sto[j].v = spcl_parse_line_rs(c, make_read_state(lb, lb_add(lb, eq_loc, 1), inds[i+1]), NULL);
	} else {
	    sto[j].s = NULL;
	    sto[j].v = spcl_parse_line_rs(c, make_read_state(lb, lb_add(lb, inds[i], 1), inds[i+1]), NULL);
	}
	//check for errors
	if (sto[i].v.type == VAL_ERR) {
	    value er = spcl_copy_val(sto[i].v);
	    return er;
	}
	//only include defined values in the argument list
	if (sto[j].v.type != VAL_UNDEF)
	    ++j;
    }
    *n = j;
    return spcl_make_none();
}

//parse function definition/call statements
static inline value parse_literal_func(struct spcl_inst* c, read_state rs, lbi open_ind, lbi close_ind, lbi* new_end) {
    //check if this is a parenthetical expression
    while ( is_whitespace(lb_get(rs.b, rs.start)) && lbicmp(rs.start, open_ind) )
	rs.start = lb_add(rs.b, rs.start, 1);
    if (lbicmp(rs.start, open_ind) == 0)
	return spcl_parse_line_rs(c, make_read_state(rs.b, lb_add(rs.b, open_ind, 1), close_ind), NULL);
    value sto = spcl_make_none();
    rs.end = close_ind;

    //read the indices
    spcl_func_call f;
    lbi* arg_inds = csv_to_inds(rs.b, open_ind, close_ind, &f.n_args);
    if (f.n_args == 0)
	return spcl_make_err(E_BAD_SYNTAX, "overlapping parentheses (something really weird happened)");
    if (f.n_args >= ARGS_BUF_SIZE)
	return spcl_make_err(E_OUT_OF_RANGE, "spclang only supports at most %lu arguments in functions", ARGS_BUF_SIZE-1);
    //set up a function and figure out the function name 
    lbi s = find_token_before(rs.b, open_ind, rs.start);
    if (s.line != open_ind.line)
	return spcl_make_err(E_BAD_SYNTAX, "unexpected line break");
    f.name = lb_read(rs.b, s);
    //check if this is a declaration
    lbi func_start = token_block(rs.b, rs.start, open_ind, "func", strlen("func"));
    if (lbicmp(func_start, open_ind) < 0) {
	//handle function declarations
	sto.type = VAL_FUNC;
	sto.n_els = f.n_args;
	//we setup arg_inds such that consecutive indices give strings to read
	for (size_t i = 0; i < f.n_args && i+1 < ARGS_BUF_SIZE; ++i)
	    f.args[i].s = lb_get_line(rs.b, lb_add(rs.b, arg_inds[i], 1), arg_inds[i+1], NULL);
	sto.val.f = make_spcl_uf_lb(f, lb_get_enclosed(rs.b, close_ind, new_end, '{', '}', 0, 0));
    } else {
	//isdef is a special function, we implement it here to avoid errors about potentially undefined values
	if (namecmp(f.name, "isdef", strlen("isdef")) == 0) {
	    if (f.n_args == 0)
		return spcl_make_err(E_LACK_TOKENS, "isdef() expected 1 argument, got 0");
	    sto = spcl_find_rs( c, make_read_state(rs.b, lb_add(rs.b, arg_inds[0], 1), arg_inds[1]) );
	    free(arg_inds);
	    if (sto.type == VAL_UNDEF)
		return spcl_make_num(0);
	    else
		return spcl_make_num(1);
	}
	//handle function calls
	value func_val = spcl_find_rs(c, make_read_state(rs.b, s, open_ind));
	if (func_val.type != VAL_FUNC) {
	    spcl_cleanup_func_call(&f);
	    free(arg_inds);
	    return spcl_make_err(E_LACK_TOKENS, "unrecognized function name %s\n", f.name);
	}
	sto = inds_to_nvp(c, rs.b, f.args, arg_inds, &f.n_args, 0);
	if (sto.type == VAL_ERR) {
	    spcl_cleanup_func_call(&f);
	    free(arg_inds);
	    return sto;
	}
	sto = spcl_uf_eval(func_val.val.f, c, f);
    }
    spcl_cleanup_func_call(&f);
    free(arg_inds);
    return sto;
}

//parse table declaration statements
static inline value parse_literal_table(struct spcl_inst* c, read_state rs, lbi open_ind, lbi close_ind) {
    value ret;
    //read the indices
    lbi* inds = csv_to_inds(rs.b, open_ind, close_ind, &ret.n_els);
    if (ret.n_els == 0)
	return spcl_make_err(E_BAD_SYNTAX, "overlapping parentheses (something really weird happened)");
    //allocate memory to hold the nvps
    name_val_pair* nvps = malloc(sizeof(name_val_pair)*ret.n_els);
    ret.val.c = make_spcl_inst(c);
    if (!nvps || !ret.val.c)
	return spcl_make_err(E_NOMEM, "");
    //read each value
    value er = inds_to_nvp(c, rs.b, nvps, inds, &ret.n_els, 1);
    if (er.type == VAL_ERR) {
	destroy_spcl_inst(ret.val.c);
	free(inds);
	return er;
    }
    //transfer each value
    for (size_t i = 0; i < ret.n_els; ++i) {
	spcl_set_value(ret.val.c, nvps[i].s, nvps[i].v, 0);
	free(nvps[i].s);
    }
    ret.type = VAL_INST;
    free(nvps);
    free(inds);
    return ret;
}

/**
 * Parse a value from the line buffer rs.b
 * c: the spcl_inst to use for function calls and variables etc.
 * rs: the current state to read, includes the buffer and the start and end indices
 * new_end: if non-null, save the last character read when parsing this line
 */
static inline value spcl_parse_line_rs(struct spcl_inst* c, read_state rs, lbi* new_end) {
    if (new_end)
	*new_end = rs.end;
    value sto = spcl_make_none();
    //iterate until we hit a non whitespace character
    int is_var = is_name(&rs);
    if (lbicmp(rs.start, rs.end) >= 0)
	return sto;

    //store locations of the first instance of different operators. We do this so we can quickly look up new operators if we didn't find any other operators of a lower precedence (such operators are placed in the tree first).
    lbi open_ind, close_ind, op_loc;
    sto = find_operator(rs, &op_loc, &open_ind, &close_ind, new_end);
    if (new_end) rs.end = *new_end;
    if (sto.type == VAL_ERR)
	return sto;

    //last try removing parenthesis 
    if (lbicmp(op_loc, rs.end) >= 0) {
	//if there isn't a valid parenthetical expression, then we should interpret this as a variable
	if (lbicmp(open_ind, rs.end) == 0 || lbicmp(close_ind, rs.end) == 0) {
	    //ensure that empty strings return undefined
	    while (is_whitespace(lb_get(rs.b, rs.start)) && lbicmp(rs.start, rs.end) < 0)
		rs.start = lb_add(rs.b, rs.start, 1);
	    char cur = lb_get(rs.b, rs.start);
	    if (cur == 0 || lbicmp(rs.start, rs.end) == 0)
		return spcl_make_none(); 
	    if (is_var) {
		//spcl_find variables
		sto = spcl_copy_val( spcl_find_rs(c, make_read_state(rs.b, rs.start, rs.end)) );
	    } else {
		//interpret number literals
		errno = 0;
		char* str = lb_get_line(rs.b, rs.start, rs.end, NULL);
		sto.val.x = strtod(str, NULL);
		sto.n_els = 1;
		if (errno) {
		    sto = spcl_make_err(E_UNDEF, "undefined token %s", str);
		    free(str);
		    return sto;
		}
		free(str);
		sto.type = VAL_NUM;
	    }
	} else {
	    //if there are enclosed blocks then we need to read those
	    switch (lb_get(rs.b, open_ind)) {
	    case '\"': sto.type = VAL_STR; sto.val.s = lb_get_line(rs.b, lb_add(rs.b, open_ind, 1), close_ind, &sto.n_els);sto.n_els += 1;break; //"
	    case '[':  return (is_var)? spcl_copy_val(spcl_find_rs(c, rs)) : parse_literal_list(c, rs, open_ind, close_ind);break; //]
	    case '{':  return parse_literal_table(c, rs, open_ind, close_ind);break; //}
	    case '(':  return parse_literal_func(c, rs, open_ind, close_ind, new_end);break; //)
	    }
	}
    } else {
	return do_op(c, rs, op_loc);
    }
    return sto;
}
value spcl_parse_line(spcl_inst* c, char* str) {
    //Setup a dummy line buffer. We're calling alloca with sizes known at compile-time, don't get mad at me.
    spcl_line_buffer lb;
    lb.lines = alloca(sizeof(char*));
    lb.line_sizes = alloca(sizeof(size_t));
    lb.lines[0] = str;
    lb.line_sizes[0] = strlen(str);
    lb.n_lines = 1;
    return spcl_parse_line_rs(c, make_read_state(&lb, make_lbi(0,0), make_lbi(0, lb.line_sizes[0])), NULL);
}
value spcl_find(const struct spcl_inst* c, const char* str) {
    //Setup a dummy line buffer. We're calling alloca with sizes known at compile-time, don't get mad at me.
    spcl_line_buffer lb;
    lb.lines = alloca(sizeof(char*));
    lb.line_sizes = alloca(sizeof(size_t));
    lb.lines[0] = str;
    lb.line_sizes[0] = strlen(str);
    lb.n_lines = 1;
    return spcl_find_rs( c, make_read_state(&lb, make_lbi(0,0), make_lbi(0, lb.line_sizes[0])) );
}
int spcl_find_object(const spcl_inst* c, const char* str, const char* typename, spcl_inst** sto) {
    value vobj = spcl_find(c, str);
    if (vobj.type != VAL_INST)
	return -1;
    value tmp = spcl_find(vobj.val.c, "__type__");
    if (tmp.type != VAL_STR || strcmp(tmp.val.s, typename))
	return -2;
    if (sto) *sto = vobj.val.c;
    return 0;
}
int spcl_find_c_array(const spcl_inst* c, const char* str, double* sto, size_t n) {
    if (sto == NULL || n == 0)
	return 0;
    value tmp = spcl_find(c, str);
    if (!tmp.type)
	return -1;
    //bounds check
    size_t n_write = (tmp.n_els > n) ? n : tmp.n_els;
    if (tmp.type == VAL_ARRAY) {
	memcpy(sto, tmp.val.a, sizeof(double)*n_write);
	return (int)n_write;
    } else if (tmp.type == VAL_LIST) {
	for (size_t i = 0; i < n_write; ++i) {
	    if (tmp.val.l[i].type != VAL_NUM)
		return -3;
	    sto[i] = tmp.val.l[i].val.x;
	}
	return (int)n_write;
    }
    return -2;
}
int spcl_find_c_str(const spcl_inst* c, const char* str, char* sto, size_t n) {
    if (sto == NULL || n == 0)
	return 0;
    value tmp = spcl_find(c, str);
    if (tmp.type != VAL_STR)
	return -1;
    //bounds check
    size_t n_write = (tmp.n_els > n) ? n : tmp.n_els;
    memcpy(sto, tmp.val.s, sizeof(char)*n_write);
    return 0;
}
int spcl_find_int(const spcl_inst* c, const char* str, int* sto) {
    value tmp = spcl_find(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = (int)tmp.val.x;
    return 0;
}
int spcl_find_size(const spcl_inst* c, const char* str, size_t* sto) {
    value tmp = spcl_find(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = (size_t)tmp.val.x;
    return 0;
}
int spcl_find_float(const spcl_inst* c, const char* str, double* sto) {
    value tmp = spcl_find(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = tmp.val.x;
    return 0;
}
void spcl_set_value(struct spcl_inst* c, const char* p_name, value p_val, int copy) {
    //generate a fake name if none was provided
    if (!p_name || p_name[0] == 0) {
	char tmp[BUF_SIZE];
	snprintf(tmp, BUF_SIZE, "\e_%lu", c->n_memb);
	return spcl_set_value(c, tmp, p_val, copy);
    }
    size_t ti = fnv_1(p_name, SIZE_MAX, c->t_bits);
    if (!find_ind(c, p_name, SIZE_MAX, &ti)) {
	//if there isn't already an element with that name we have to expand the table and add a member
	if (grow_inst(c))
	    find_ind(c, p_name, SIZE_MAX, &ti);
	c->table[ti].s = strdup(p_name);
	c->table[ti].v = (copy)? spcl_copy_val(p_val) : p_val;
	++c->n_memb;
    } else {
	//otherwise we need to cleanup the old value and add the new
	spcl_cleanup_val( &(c->table[ti].v) );
	c->table[ti].v = (copy)? spcl_copy_val(p_val) : p_val;
    }
}

value spcl_read_lines(struct spcl_inst* c, const spcl_line_buffer* b) {
    value er = spcl_make_none();

    lbi end;
    read_state rs = make_read_state(b, make_lbi(0,0), make_lbi(0,0));
    //iterate over each line in the file
    while (lbicmp(rs.start, lb_end(rs.b)) < 0) {
	rs.end = make_lbi(rs.start.line, b->line_sizes[rs.start.line]);
	er = spcl_parse_line_rs(c, rs, &end);
	if (er.type == VAL_ERR) {
	    if (c->parent == NULL)
		fprintf(stderr, "Error %s on line %lu: %s\n", errnames[er.val.e->c], rs.start.line+1, er.val.e->msg);
	    return er;
	}
	spcl_cleanup_val(&er);
	rs.start = lb_add(b, end, 1);
    }
    return er;
}

spcl_inst* spcl_inst_from_file(const char* fname, value* error, int argc, char** argv) {
    //create a new spcl_inst
    spcl_inst* ret = make_spcl_inst(NULL);
    if (!ret)
	return ret;
    //read command line arguments
    value er;
    for (int i = 0; argv && i < argc; ++i) {
	er = spcl_parse_line(ret, argv[i]);
	if (er.type == VAL_ERR) {
	    fprintf(stderr, "Error %s in option %d: %s\n", errnames[er.val.e->c], i, er.val.e->msg);
	    if (error)
		*error = er;
	}
    }
    //read the rest of the file
    spcl_line_buffer* lb = make_spcl_line_buffer(fname);
    er = spcl_read_lines(ret, lb);
    if (error)
	*error = er;
    destroy_spcl_line_buffer(lb);
    return ret;
}

/** ============================ spcl_uf ============================ **/

spcl_uf* make_spcl_uf_lb(spcl_func_call sig, spcl_line_buffer* b) {
    spcl_uf* uf = (spcl_uf*)malloc(sizeof(spcl_uf));
    uf->code_lines = copy_spcl_line_buffer(b);
    uf->call_sig = spcl_copy_func_call(sig);
    uf->exec = NULL;
    return uf;
}
spcl_uf* make_spcl_uf_ex(value (*p_exec)(spcl_inst*, spcl_func_call)) {
    spcl_uf* uf = (spcl_uf*)malloc(sizeof(spcl_uf));
    uf->code_lines = NULL;
    uf->call_sig.name = NULL;
    uf->call_sig.n_args = 0;
    uf->exec = p_exec;
    return uf;
}
spcl_uf* copy_spcl_uf(const spcl_uf* o) {
    spcl_uf* uf = (spcl_uf*)malloc(sizeof(spcl_uf));
    uf->code_lines = (o->code_lines)? copy_spcl_line_buffer(o->code_lines): NULL;
    uf->call_sig.name = (o->call_sig.name)? strdup(o->call_sig.name): NULL;
    uf->call_sig.n_args = o->call_sig.n_args;
    uf->exec = o->exec;
    return uf;
}
//deallocation
void destroy_spcl_uf(spcl_uf* uf) {
    spcl_cleanup_func_call(&(uf->call_sig));
    if (uf->code_lines)
	destroy_spcl_line_buffer(uf->code_lines);
    free(uf);
}
/*const char* token_names[] = {"if", "for", "while", "return"};
typedef enum {TOK_NONE, TOK_IF, TOK_FOR, TOK_WHILE, TOK_RETURN, N_TOKS} token_type;*/
value spcl_uf_eval(spcl_uf* uf, spcl_inst* c, spcl_func_call call) {
    if (uf->exec) {
	return (*uf->exec)(c, call);
    } else if (uf->call_sig.name && uf->call_sig.n_args > 0) {
	if (call.n_args != uf->call_sig.n_args)
	    return spcl_make_err(E_LACK_TOKENS, "%s() expected %lu arguments, got %lu", uf->call_sig.name, call.n_args, uf->call_sig.name);
	//setup a new scope with function arguments defined
	spcl_inst* func_scope = make_spcl_inst(c);
	for (size_t i = 0; i < uf->call_sig.n_args; ++i) {
	    spcl_set_value(func_scope, uf->call_sig.args[i].s, call.args[i].v, 0);
	}
	destroy_spcl_inst(func_scope);
    }
    return spcl_make_err(E_BAD_VALUE, "function not implemented");
    /*er = E_SUCCESS;
    size_t lineno = 0;
    value bad_val;
    while (lineno < code_lines.get_n_lines()) {
	char* line = code_lines.get_line(lineno);
	//search for tokens
	token_type this_tok = TOK_NONE;
	char* token = NULL;
	for (size_t i = 0; i < N_TOKS; ++i) {
	    token = token_block(line, token_names[i]);
	    if (token) {
		this_tok = (token_type)i;
	    }
	}
	switch (this_tok) {
	    //TODO
	    defualt: break;
	}
	free(line);
    }
    //return an undefined value by default
    value ret;ret.type = VAL_UNDEF;ret.val.x = 0;return ret;*/
}
