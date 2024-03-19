#include "read.h"

/**
 * iterate through a context by finding defined entries in the table
 */
static inline size_t con_it_next(const context* c, size_t i) {
    for (; i < con_size(c); ++i) {
	if (c->table[i].val.type)
	    return i;
    }
    return i;
}

STACK_DEF(blk_type)
STACK_DEF(size_t)

/** ======================================================== utility functions ======================================================== **/

/**
 * check if a character is whitespace
 */
static inline int is_whitespace(char c) {
    if (c == 0 || c == ' ' || c == '\t' || c == '\n' || c == '\r')
	return 1;
    return 0;
}

/**
 * Helper function for is_token which tests whether the character c is a token terminator
 */
static inline int is_char_sep(char c) {
    if (is_whitespace(c) || c == ';' || c == '+'  || c == '-' || c == '*'  || c == '/')
	return 1;
    return 0;
}

/**
 * Helper function which looks at the string str at index i and tests whether it is a token with the matching name
 */
static inline int is_token(const char* str, size_t i, size_t len) {
    if (i > 0 && !is_char_sep(str[i-1]))
	return 0;
    if (!is_char_sep(str[i+len]))
	return 0;
    return 1;
}

/**
 * Helper function that finds the start of first token before the index i in the string str. Note that this is not null terminated and includes characters including and after str[i] (unless str[i] = 0).
 */
static inline char* find_token_before(char* str, size_t i) {
    while (i > 0) {
	--i;
	if (is_char_sep(str[i])) return str+i+1;
    }
    return str;
}

/**
 * Find the index of the first character c that isn't nested inside a block or NULL if an error occurred
 */
static inline char* strchr_block(char* str, char c) {
    stack(size_t) blk_stk = make_stack(size_t)();
    size_t j;
    for (size_t i = 0; str[i] != 0; ++i) {
	if (str[i] == c && blk_stk.ptr == 0) {
	    destroy_stack(size_t)(&blk_stk, NULL);
	    return str+i;
	}
	if (str[i] == '('/*)*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*(*/')') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (str[i] == '['/*]*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*[*/']') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (str[i] == '{'/*}*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*{*/'}') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (str[i] == '\"'/*"*/) {
	    //quotes are more complicated
	    if (blk_stk.ptr > 0 && !peek(size_t)(&blk_stk, 1, &j) && str[j] == '\"') {
		if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	    } else {
		if ( push(size_t)(&blk_stk, i) ) goto exit;
	    }
	} else if (str[i] == '\''/*"*/) {
	    //quotes are more complicated
	    if (blk_stk.ptr > 0 && !peek(size_t)(&blk_stk, 1, &j) && str[j] == '\'') {
		if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	    } else {
		if ( push(size_t)(&blk_stk, i) ) goto exit;
	    }
	}
    }
exit:
    destroy_stack(size_t)(&blk_stk, NULL);
    return NULL;
}

char* token_block(char* str, const char* comp) {
    if (!str || !comp) return NULL;
    stack(size_t) blk_stk = make_stack(size_t)();
    size_t j;
    for (size_t i = 0; str[i] != 0; ++i) {
	if (blk_stk.ptr == 0) {
	    j = 0;
	    while (comp[j] && str[i+j] && str[i+j] == comp[j]) ++j;
	    if (comp[j] == 0 && is_token(str, i, j)) {
		destroy_stack(size_t)(&blk_stk, NULL);
		return str+i;
	    }
	}
	if (str[i] == '('/*)*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*(*/')') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (str[i] == '['/*]*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*[*/']') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (str[i] == '{'/*}*/) {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == /*{*/'}') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (i > 0 && str[i-1] == '/' && str[i] == '*') {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (i > 0 && str[i-1] == '*' && str[i] == '/') {
	    if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	} else if (i > 0 && str[i-1] == '/' && str[i] == '/') {
	    if ( push(size_t)(&blk_stk, i) ) goto exit;
	} else if (str[i] == '\"'/*"*/) {
	    //quotes are more complicated 
	    if (blk_stk.ptr != 0 && !peek(size_t)(&blk_stk, 1, &j) && str[j] == '\"') {
		if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	    } else {
		if ( push(size_t)(&blk_stk, i) ) goto exit;
	    }
	} else if (str[i] == '\''/*"*/) {
	    //quotes are more complicated
	    if (blk_stk.ptr != 0 && !peek(size_t)(&blk_stk, 1, &j) && str[j] == '\'') {
		if ( pop(size_t)(&blk_stk, NULL) ) goto exit;
	    } else {
		if ( push(size_t)(&blk_stk, i) ) goto exit;
	    }
	}
    }
exit:
    destroy_stack(size_t)(&blk_stk, NULL);
    return NULL;
}

int write_numeric(char* str, size_t n, double x) {
    if (x >= 1000000)
	return snprintf(str, n, "%e", x);
    return snprintf(str, n, "%f", x);
}

char* trim_whitespace(char* str, size_t* len) {
    if (!str) return NULL;
    size_t start_ind = 0;
    int started = 0;
    _uint last_non = 0;
    for (_uint i = 0; str[i] != 0; ++i) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') {
            last_non = i;
            if (!started) {
                start_ind = i;
		started = 1;
            }
        }
    }
    str[last_non+1] = 0;
    if (len) *len = last_non - start_ind;
    return str+start_ind;
}

char** csv_to_list(char* str, char sep, size_t* listlen) {
    char** ret = NULL;/*(char**)malloc(sizeof(char*), &tmp_err);*/
    //we don't want to include separators that are in nested environments i.e. if the input is [[a,b],c] we should return "[a,b]","c" not "[a","b]","c"
    stack(blk_type) blk_stk = make_stack(blk_type)();
    blk_type tmp;

    //by default we ignore whitespace, only use it if we are in a block enclosed by quotes
    char* saveptr = str;
    size_t off = 0;
    size_t j = 0;
    size_t i = 0;
    int verbatim = 0;

    for (; str[i] != 0; ++i) {
	//if this is a separator then add the entry to the list
	if (str[i] == sep && blk_stk.ptr == 0) {
	    ret = (char**)realloc(ret, sizeof(char*)*(off+1));

	    //append the element to the list
	    ret[off++] = saveptr;
	    //null terminate this string and increment j
	    str[j++] = 0;
	    saveptr = str + j;
	} else {
	    if (str[i] == '\\') {
		//check for escape sequences
		++i;
		int placed_escape = 0;
		switch (str[i]) {
		case 'n': str[j++] = '\n';placed_escape = 1;break;
		case 't': str[j++] = '\t';placed_escape = 1;break;
		case '\\': str[j++] = '\\';placed_escape = 1;break;
		case '\"': str[j++] = '\"';placed_escape = 1;break;
		default: goto exit;
		}
		if (placed_escape) continue;
	    } else if (str[i] == '\"') {
		int code = peek(blk_type)(&blk_stk, 1, &tmp);
		if (code || tmp != BLK_QUOTE) {
		    if ( push(blk_type)(&blk_stk, BLK_QUOTE) ) goto exit;
		    verbatim = 1;
		} else {
		    pop(blk_type)(&blk_stk, &tmp);
		    verbatim = 0;
		}
	    } else if (str[i] == '['/*]*/) {
		if ( push(blk_type)(&blk_stk, BLK_SQUARE) ) goto exit;
	    } else if (str[i] == /*[*/']') {
		//don't fail if we reach the end of a block. This just means we've reached the end of the list
		if ( pop(blk_type)(&blk_stk, &tmp) ) break;
		if (tmp != BLK_SQUARE)
		    goto exit;
	    } else if (str[i] == '('/*)*/) {
		if ( push(blk_type)(&blk_stk, BLK_PAREN) ) goto exit;
	    } else if (str[i] == /*(*/')') {
		if ( pop(blk_type)(&blk_stk, &tmp) ) break;
		if (tmp != BLK_PAREN)
		    goto exit;
	    } else if (str[i] == '{'/*}*/) {
		if ( push(blk_type)(&blk_stk, BLK_CURLY) ) goto exit;
	    } else if (str[i] == /*{*/'}') {
		if ( pop(blk_type)(&blk_stk, &tmp) ) break;
		if (tmp != BLK_CURLY)
		    goto exit;
	    }
	    if (blk_stk.ptr || verbatim || (str[i] != ' ' && str[i] != '\t' && str[i] != '\n')) {
		//if this isn't whitespace then just copy the character
		str[j++] = str[i];
	    }
	}
    }
    //make sure that there weren't any unterminated blocks
    if (blk_stk.ptr)
	goto exit;

    //make sure the string is null terminated
    //if (str[i] != 0) str[j] = 0;
    str[j] = 0;
    ret = (char**)realloc(ret, sizeof(char*)*(off+2));
    //add the last element to the list, but only if something was actually written, then set the length if requested
    if (j != 0) ret[off++] = saveptr;
    if (listlen) *listlen = off;
    ret[off] = NULL;
    destroy_stack(blk_type)(&blk_stk, NULL);
    return ret;
exit:
    free(ret);
    destroy_stack(blk_type)(&blk_stk, NULL);
    return NULL;
}

char* append_to_line(char* line, size_t* line_off, size_t* line_size, const char* str, size_t n) {
    if (!line_off || !line_size || !str || n == 0) return line;
    size_t ls = *line_size;
    size_t i = *line_off;
    if (!line || i + n >= ls) {
	ls = 2*i + n + 1;
	line = (char*)realloc(line, ls);
    }
    size_t j = 0;
    for (; j < n; ++j) {
	line[i+j] = str[j];
	//terminate on reaching the string end
	if (!str[j]) {
	    ++j;
	    break;
	}
    }
    *line_off = i+j;
    *line_size = ls;
    return line;
}

/** ============================ line_buffer_ind ============================ **/

//create a new line buffer with the given line index and offset
line_buffer_ind make_lbi(size_t p_line, size_t p_off) {
    line_buffer_ind ret;
    ret.line = p_line;
    ret.off = p_off;
    return ret;
}
//increase the line buffer lhs by rhs characters while keeping line number the same
line_buffer_ind lbi_add(line_buffer_ind lhs, size_t rhs) {
    line_buffer_ind ret;
    ret.line = lhs.line;
    ret.off = lhs.off + rhs;
    return ret;
}

//decrease the line buffer lhs by rhs characters while keeping line number the same
line_buffer_ind lbi_sub(line_buffer_ind lhs, size_t rhs) {
    line_buffer_ind ret;
    ret.line = lhs.line;
    if (rhs > lhs.off)
        ret.off = 0;
    else
        ret.off = lhs.off - rhs;
    return ret;
}

/** ============================ line_buffer ============================ **/

/**
 * Return a new line_buffer
 */
static inline line_buffer* alloc_line_buffer() {
    line_buffer* lb = (line_buffer*)malloc(sizeof(line_buffer));
    lb->lines = NULL;
    lb->line_sizes = NULL;
    lb->n_lines = 0;
    return lb;
}

/**
 * Free memory associated with the line_buffer lb
 */
void destroy_line_buffer(line_buffer* lb) {
    if (lb->lines) {
        for (size_t i = 0; i < lb->n_lines; ++i)
            free(lb->lines[i]);
        free(lb->lines);
    }
    if (lb->line_sizes)
        free(lb->line_sizes);
    free(lb);
}

line_buffer* make_line_buffer_file(const char* p_fname) {
    line_buffer* lb = alloc_line_buffer();
    size_t buf_size = 1024;
    lb->lines = (char**)malloc(sizeof(char*) * buf_size);
    lb->line_sizes = (size_t*)malloc(sizeof(size_t) * buf_size);
    lb->n_lines = 0;
    FILE* fp = NULL;
    if (p_fname)
        fp = fopen(p_fname, "r");
    if (fp) {
        size_t lineno = 1;
        size_t line_len = 0;
        int go_again = 1;
        do {
	    //reallocate buffer if necessary
            if (lb->n_lines >= buf_size) {
                buf_size *= 2;
                lb->lines = (char**)realloc(lb->lines, sizeof(char*) * buf_size);
                lb->line_sizes = (size_t*)realloc(lb->line_sizes, sizeof(size_t) * buf_size);
            }
	    //read the line until a semicolon, newline or EOF is found
            size_t this_size = 1024;
            char* this_buf = (char*)malloc(this_size);
            int res = fgetc(fp);
            for (line_len = 0; 1; ++line_len) {
                if (line_len >= this_size) {
                    this_size *= 2;
                    this_buf = (char*)realloc(this_buf, sizeof(char) * this_size);
                }
                if (res == EOF || (char)res == ';' || (char)res == '\n') {
                    this_buf[line_len] = 0;
                    if ((char)res == '\n')
                        ++lineno;
                    else if ((char)res == EOF)
                        go_again = 0;
                    line_len = line_len;
                    break;
                }
                this_buf[line_len] = (char)res;
                res = fgetc(fp);
            }
            if (line_len > 0) {
                this_buf = (char*)realloc(this_buf, sizeof(char) * (line_len + 1));
                lb->lines[lb->n_lines] = this_buf;
                lb->line_sizes[lb->n_lines++] = line_len;
            } else {
                free(this_buf);
            }
        } while (go_again);
        lb->lines = (char**)realloc(lb->lines, sizeof(char*) * lb->n_lines);
        lb->line_sizes = (size_t*)realloc(lb->line_sizes, sizeof(size_t) * lb->n_lines);
        fclose(fp);
    } else {
        printf("Error: couldn't open file %s for reading!\n", p_fname);
        free(lb->lines);
        lb->lines = NULL;
    }
    return lb;
}

/**
 * Create a line buffer from a single line separated by characters of the type sep such that sep are not contained inside any blocks specified by ignore_blocks. Seperation characters are not included.
 */
line_buffer* make_line_buffer_lines(const char** p_lines, size_t pn_lines) {
    line_buffer* lb = alloc_line_buffer();
    lb->n_lines = pn_lines;
    lb->lines = (char**)malloc(sizeof(char*) * lb->n_lines);
    lb->line_sizes = (size_t*)malloc(sizeof(size_t) * lb->n_lines);
    for (size_t i = 0; i < lb->n_lines; ++i) {
        lb->line_sizes[i] = strlen(p_lines[i]);
        lb->lines[i] = strdup(p_lines[i]);
    }
    return lb;
}

/**
 * Create a line buffer from a single line separated by characters of the type sep such that sep are not contained inside any blocks specified by ignore_blocks. Seperation characters are not included.
 */
line_buffer* make_line_buffer_sep(const char* line, char sep) {
    line_buffer* lb = alloc_line_buffer();
    lb->n_lines = 0;
    static const char* ignore_blocks = "\"\"()[]{}";
    //by doing this we allocate once since we have a guaranteed upper limit on the number of lines that might exist
    size_t line_buf_size = strlen(line);
    lb->lines = (char**)malloc(sizeof(char*) * line_buf_size);
    lb->line_sizes = (size_t*)malloc(sizeof(size_t) * line_buf_size);
    int nest_level = 0;
    size_t last_sep = 0;
    size_t i = 0;
    for (;; ++i) {
	//iterate through pairs of open/close block delimeters
        for (size_t j = 0; ignore_blocks[j] && ignore_blocks[j + 1]; j += 2) {
            if (line[i] == ignore_blocks[j] && ignore_blocks[j] == ignore_blocks[j + 1]) {
		//there's a special case for things like quotations, skip ahead until we find the end
                ++i;
                for (; line[i] && line[i] != ignore_blocks[j]; ++i)
                    ;
                break;
            } else if (line[i] == ignore_blocks[j]) {
                ++nest_level;
            } else if (line[i] == ignore_blocks[j + 1]) {
                --nest_level;
            }
        }
        if (nest_level <= 0 && (line[i] == sep || line[i] == 0)) {
            lb->line_sizes[lb->n_lines] = i - last_sep;
            lb->lines[lb->n_lines] = (char*)malloc(sizeof(char) * (lb->line_sizes[lb->n_lines] + 1));
            for (size_t j = last_sep; j < i; ++j) {
                lb->lines[lb->n_lines][j - last_sep] = line[j];
            }
            lb->lines[lb->n_lines][lb->line_sizes[lb->n_lines]] = 0;
            last_sep = i + 1;
            ++lb->n_lines;
            if (line[i] == 0)
                break;
        }
    }
    return lb;
}

line_buffer* copy_line_buffer(const line_buffer* o) {
    line_buffer* lb = alloc_line_buffer();
    lb->n_lines = o->n_lines;
    lb->line_sizes = (size_t*)malloc(sizeof(size_t) * lb->n_lines);
    lb->lines = (char**)malloc(sizeof(char*) * lb->n_lines);
    for (size_t i = 0; i < lb->n_lines; ++i) {
        lb->lines[i] = strdup(o->lines[i]);
        lb->line_sizes[i] = o->line_sizes[i];
    }
    return lb;
}

/**
 * Goes through a line buffer and splits into multiple lines at each instance of split_delim.
 * i.e. if the buffer is in the state
 * lines = {"foo; bar", "foobar;"}
 * then split(';') will transform the state to
 * lines = {"foo", " bar", "foobar;"}
 */
void lb_split(line_buffer* lb, char split_delim) {
    for (size_t i = 0; i < lb->n_lines; ++i) {
        for (size_t j = 0; j < lb->line_sizes[i]; ++j) {
            if (lb->lines[i][j] == split_delim) {
		//find matches
                lb->n_lines++;
                lb->lines = (char**)realloc(lb->lines, sizeof(char*) * lb->n_lines);
                lb->line_sizes = (size_t*)realloc(lb->line_sizes, sizeof(size_t) * lb->n_lines);
		//move everything else forward
                for (size_t k = lb->n_lines - 1; k > i + 1; --k) {
                    lb->lines[k] = lb->lines[k - 1];
                    lb->line_sizes[k] = lb->line_sizes[k - 1];
                }
		//split the line
                lb->lines[i + 1] = strndup(lb->lines[i] + j + 1, lb->line_sizes[i] - j);
                lb->lines[i][j] = 0;
                lb->line_sizes[i + 1] = lb->line_sizes[i] - j - 1;
                lb->line_sizes[i] = j;
            }
        }
    }
}

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
#if DEBUG_INFO<1
static inline
#endif
int it_single(const line_buffer* lb, char** linesto, char start_delim, char end_delim, line_buffer_ind* start, line_buffer_ind* end, int* pdepth, int include_delims, int include_start) {
    int free_after = 0;
    if (start == NULL) {
        start = (line_buffer_ind*)malloc(sizeof(line_buffer_ind));
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
line_buffer* lb_get_enclosed(const line_buffer* lb, line_buffer_ind start, line_buffer_ind* pend, char start_delim, char end_delim, int include_delims, int include_start) {
    line_buffer* ret = alloc_line_buffer();
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
    line_buffer_ind end;
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

/**
 * Jump to the end of the next enclosed block started with a start_delim character
 * lb: the linebuffer
 * start: the location from which we start seeking
 * start_delim: the starting delimeter to look for (e.g. '(','{'... corresponding to ')','}'... respectively)
 * end_delim: the ending delimiter to look for, see above
 * include_delims: if true, then include the delimeter in the libe buffer
 */
line_buffer_ind lb_jmp_enclosed(line_buffer* lb, line_buffer_ind start, char start_delim, char end_delim, int include_delims) {
    int depth = 0;
    for (size_t i = start.line; depth >= 0 && i < lb->n_lines; ++i) {
        line_buffer_ind end;
        end.line = i;
        end.off = 0;
        it_single(lb, NULL, start_delim, end_delim, &start, &end, &depth, include_delims, 0);
        if (end.line == start.line)
            return end;
        start.off = 0;
        ++start.line;
    }
    line_buffer_ind ret;
    ret.line = lb->n_lines;
    ret.off = 0;
    return ret;
}

/**
 * Return a string with the line contained at index i. This string should be freed with a call to free().
 */
char* lb_get_line(const line_buffer* lb, line_buffer_ind p) {
    if (p.line >= lb->n_lines) {
        return NULL;
    }
    size_t line_size = lb->line_sizes[p.line] + 1;
    char* ret = (char*)malloc(sizeof(char) * (line_size - p.off));
    for (size_t j = p.off; j < line_size; ++j)
        ret[j - p.off] = lb->lines[p.line][j];
    ret[line_size - p.off - 1] = 0;
    return ret;
}
/**
 * Return the size of the line with index i.
 */
size_t lb_get_line_size(const line_buffer* lb, size_t i) {
    if (i >= lb->n_lines)
	return 0;
    return lb->line_sizes[i];
}
/**
 * Returns a version of the line buffer which is flattened so that everything fits onto one line.
 * sep_char: each newline in the buffer is replaced by a sep_char, unless sep_char=0 in which no characters are inserted
 * len: a pointer which if not null will hold the length of the string including the null terminator
 */
char* lb_flatten(const line_buffer* lb, char sep_char, size_t* len) {
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

/**
 * move the line buffer forward by one character
 * p: the index to start at
 */
int lb_inc(const line_buffer* lb, line_buffer_ind* p) {
    if (p->line >= lb->n_lines)
        return 0;
    if (p->off >= lb->line_sizes[p->line]) {
        if (p->line == lb->n_lines - 1) {
            return 0;
        }
        p->off = 0;
        p->line += 1;
    } else {
        p->off += 1;
    }
    return 1;
}

/**
 * move the line buffer back by one character
 * p: the index to start at
 */
int lb_dec(const line_buffer* lb, line_buffer_ind* p) {
    if (p->line > lb->n_lines)
        return 0;
    if (p->off == 0) {
        if (p->line == 0) {
            return 0;
        }
        p->line -= 1;
        p->off = lb->line_sizes[p->line];
    } else {
        p->off -= 1;
    }
    return 1;
}

/**
 * returns the character at position pos
 */
char lb_get(const line_buffer* lb, line_buffer_ind pos) {
    if (pos.line >= lb->n_lines || pos.off >= lb->line_sizes[pos.line])
        return 0;
    return lb->lines[pos.line][pos.off];
}

/** ======================================================== builtin functions ======================================================== **/
value check_signature(cgs_func f, size_t min_args, size_t max_args, const valtype* sig) {
    if (!sig || max_args < min_args)
	return make_val_undef();
    if (f.n_args < min_args)
	return make_val_error(E_LACK_TOKENS, "%s() expected %lu arguments, got %lu", f.name, min_args, f.n_args);
    if (f.n_args > max_args)
	return make_val_error(E_LACK_TOKENS, "%s() with too many arguments, %lu", f.name, f.n_args);
    for (size_t i = 0; i < f.n_args; ++i) {
	if (sig[i] && f.args[i].val.type != sig[i])//treat undefined as allowing for arbitrary type
	    return make_val_error(E_BAD_TYPE, "%s() expected args[%lu].type=%s, got %s", f.name, i, valnames[sig[i]], valnames[f.args[i].val.type]);
	if (sig[i] > VAL_NUM && f.args[i].val.val.s == NULL)
	    return make_val_error(E_BAD_TYPE, "%s() found empty %s at args[%lu]", f.name, valnames[sig[i]], i);
    }
    return make_val_undef();
}
value get_type(struct context* c, cgs_func f) {
    value sto;
    sto.type = VAL_STR;
    if (f.n_args < 1)
	return make_val_error(E_LACK_TOKENS, "typeof() called without arguments");
    //handle instances as a special case
    if (f.args[0].val.type == VAL_INST) {
	value t = lookup(f.args[0].val.val.c, "__type__");
	if (t.type == VAL_STR) {
	    sto.n_els = t.n_els;
	    sto.val.s = malloc(sizeof(char)*(sto.n_els+1));
	    for (size_t i = 0; i < sto.n_els; ++i)
		sto.val.s[i] = t.val.s[i];
	    sto.val.s[sto.n_els] = 0;
	} else {
	    return make_val_undef();
	}
	return sto;
    }
    sto.n_els = strlen(valnames[f.args[0].val.type])+1;
    sto.val.s = strdup(valnames[f.args[0].val.type]);
    return sto;
}
value make_range(struct context* c, cgs_func f) {
    static const valtype RANGE_SIG[] = {VAL_NUM, VAL_NUM, VAL_NUM};
    value sto = check_signature(f, 1, SIGLEN(RANGE_SIG), RANGE_SIG);
    if (sto.type)
	return sto;
    double min, max, inc;
    //interpret arguments depending on how many were provided
    if (f.n_args == 1) {
	min = 0;
	max = f.args[0].val.val.x;
	inc = 1;
    } else {
	min = f.args[0].val.val.x;
	max = f.args[1].val.val.x;
	inc = 1;
    }
    if (f.n_args >= 3)
	inc = f.args[2].val.val.x;
    //make sure arguments are valid
    if ((max-min)*inc <= 0)
	return make_val_error(E_BAD_VALUE, "range(%f, %f, %f) with invalid increment", min, max, inc);
    sto.type = VAL_ARRAY;
    sto.n_els = (max - min) / inc;
    sto.val.a = (double*)malloc(sizeof(double)*sto.n_els);
    for (size_t i = 0; i < sto.n_els; ++i)
	sto.val.a[i] = i*inc + min;
    return sto;
}
value make_linspace(struct context* c, cgs_func f) {
    static const valtype LINSPACE_SIG[] = {VAL_NUM, VAL_NUM, VAL_NUM};
    value sto = check_signature(f, SIGLEN(LINSPACE_SIG), SIGLEN(LINSPACE_SIG), LINSPACE_SIG);
    if (sto.type)
	return sto;
    sto.type = VAL_ARRAY;
    sto.n_els = (size_t)(f.args[2].val.val.x);
    //prevent divisions by zero
    if (sto.n_els < 2)
	return make_val_error(E_BAD_VALUE, "cannot make linspace with size %lu", sto.n_els);
    sto.val.a = (double*)malloc(sizeof(double)*sto.n_els);
    double step = (f.args[1].val.val.x - f.args[0].val.val.x)/(sto.n_els - 1);
    for (size_t i = 0; i < sto.n_els; ++i) {
	sto.val.a[i] = step*i + f.args[0].val.val.x;
    }
    return sto;
}
STACK_DEF(value)
value flatten_list(struct context* c, cgs_func f) {
    static const valtype FLATTEN_LIST_SIG[] = {VAL_LIST};
    value sto = check_signature(f, SIGLEN(FLATTEN_LIST_SIG), SIGLEN(FLATTEN_LIST_SIG), FLATTEN_LIST_SIG);
    value cur_list = f.args[0].val;
    //flattening an empty list is the identity op.
    if (cur_list.n_els == 0 || cur_list.val.l == NULL) {
	sto.type = VAL_LIST;
	return sto;
    }
    size_t cur_st = 0;
    //this is used for estimating the size of the buffer we need. Take however many elements were needed for this list and assume each sub-list has the same number of elements
    size_t base_n_els = cur_list.n_els;
    //start with the number of elements in the lowest order of the list
    size_t buf_size = cur_list.n_els;
    sto.val.l = (value*)malloc(sizeof(value)*buf_size);
    //there may potentially be nested lists, we need to be able to find our way back to the parent and the index once we're done
    stack(value) lists = make_stack(value)();
    stack(size_t) inds = make_stack(size_t)();
    //just make sure that there's a root level on the stack to be popped out
    if ( push(value)(&lists, cur_list) ) goto exit;
    if ( push(size_t)(&inds, 0) ) goto exit;
    size_t j = 0;
    do {
	size_t i = cur_st;
	size_t start_depth = inds.ptr;
	for (; i < cur_list.n_els; ++i) {
	    if (cur_list.val.l[i].type == VAL_LIST) {
		if ( push(value)(&lists, cur_list) ) goto exit;
		if ( push(size_t)(&inds, i+1) ) goto exit;//push + 1 so that we start at the next index instead of reading the list again
		cur_list = cur_list.val.l[i];
		cur_st = 0;
		break;
	    }
	    if (j >= buf_size) {
		//-1 since we already have at least one element. no base_n_els=0 check is needed since that case will ensure the for loop is never evaluated
		buf_size += (base_n_els-1)*(i+1);
		value* tmp_val = (value*)realloc(sto.val.l, sizeof(value)*buf_size);
		if (!tmp_val) {
		    free(sto.val.l);
		    cleanup_func(&f);
		    destroy_stack(value)(&lists, &cleanup_val);
		    destroy_stack(size_t)(&inds, NULL);
		    return make_val_error(E_NOMEM, "");
		}
		sto.val.l = tmp_val;
	    }
	    sto.val.l[j++] = copy_val(cur_list.val.l[i]);
	}
	//if we reached the end of a list without any sublists then we should return back to the parent list
	if (inds.ptr <= start_depth) {
	    pop(size_t)(&inds, &cur_st);
	    pop(value)(&lists, &cur_list);
	}
    } while (lists.ptr);
    sto.type = VAL_LIST;
    sto.n_els = j;
exit:
    //the list stack only holds shallow copies, so we don't want to deallocate elements
    destroy_stack(value)(&lists, NULL);
    destroy_stack(size_t)(&inds, NULL);
    return sto;
}
value concatenate(struct context* c, cgs_func f) {
    value sto;
    if (f.n_args < 2)
	return make_val_error(E_LACK_TOKENS, "cat() expected 2 arguments but got %lu", f.n_args);
    value l = f.args[0].val;
    value r = f.args[1].val;
    size_t l1 = l.n_els;
    size_t l2 = (r.type == VAL_LIST || r.type == VAL_ARRAY)? r.n_els : 1;
    //special case for matrices, just append a new row
    if (l.type == VAL_MAT && r.type == VAL_ARRAY) {
	sto.type = VAL_MAT;
	sto.n_els = l1 + 1;
	sto.val.l = (value*)malloc(sizeof(value)*sto.n_els);
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = copy_val(l.val.l[i]);
	sto.val.l[l1] = copy_val(r);
	return sto;
    }
    //otherwise we have to do something else
    if (l.type != VAL_LIST && r.type != VAL_ARRAY)
	return make_val_error(E_BAD_TYPE, "called cat() with types <%s> <%s>", valnames[l.type], valnames[r.type]);
    sto.type = l.type;
    sto.n_els = l1 + l2;
    //deep copy the first list/array
    if (sto.type == VAL_LIST) {
	sto.val.l = (value*)malloc(sizeof(value)*sto.n_els);
	if (!sto.val.l) return make_val_error(E_NOMEM, "");
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = copy_val(l.val.l[i]);
	if (r.type == VAL_LIST) {
	    //list -> list
	    for (size_t i = 0; i < l2; ++i)
		sto.val.l[i+l1] = copy_val(r.val.l[i]);
	} else if (r.type == VAL_ARRAY) {
	    //array -> list
	    for (size_t i = 0; i < l2; ++i)
		sto.val.l[i+l1] = make_val_num(r.val.a[i]);
	} else {
	    //anything -> list
	    sto.val.l[l1] = copy_val(r);
	}
    } else {
	sto.val.a = (double*)malloc(sizeof(double)*sto.n_els);
	if (!sto.val.a) return make_val_error(E_NOMEM, "");
	for (size_t i = 0; i < l1; ++i)
	    sto.val.l[i] = copy_val(f.args[0].val.val.l[i]);
	if (r.type == VAL_LIST) {
	    //list -> array
	    for (size_t i = 0; i < l2; ++i) {
		if (r.val.l[i].type != VAL_NUM) {
		    free(sto.val.a);
		    return make_val_error(E_BAD_TYPE, "can only concatenate numeric lists to arrays");
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
	    return make_val_error(E_BAD_TYPE, "called cat() with types <%s> <%s>", valnames[l.type], valnames[r.type]);
	}
    }
    return sto;
}
/**
 * print the elements to the console
 */
value print(struct context* c, cgs_func f) {
    value ret = make_val_undef();
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].val.type == VAL_NUM) {
	    printf("%f", f.args[i].val.val.x);
	} else if (f.args[i].val.type == VAL_STR) {
	    printf("%s", f.args[i].val.val.s);
	} else {
	    printf("<object at %p>", f.args[i].val.val.l);
	}
    }
    printf("\n");
    return ret;
}
/**
 * Make a vector argument with the x,y, and z coordinates supplied
 */
value make_array(context* c, cgs_func f) {
    static const valtype ARRAY_SIG[] = {VAL_LIST};
    value sto = check_signature(f, SIGLEN(ARRAY_SIG), SIGLEN(ARRAY_SIG), ARRAY_SIG);
    if (sto.type)
	return sto;

    //treat matrices with one row as vectors
    if (f.n_args == 1) {
	if (f.args[0].val.val.l[0].type == VAL_LIST)
	    return cast_to(f.args[0].val, VAL_MAT);
	else
	    return cast_to(f.args[0].val, VAL_ARRAY);
    }
    //otherwise we need to do more work
    size_t n_cols = f.args[0].val.n_els;
    sto.type = VAL_MAT;
    sto.n_els = f.n_args;
    sto.val.l = (value*)malloc(sizeof(value)*f.n_args);
    //iterate through rows
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].val.type == VAL_LIST) { free(sto.val.l);return make_val_error(E_BAD_TYPE, "non list encountered in matrix"); }
	if (f.args[i].val.n_els != n_cols) { free(sto.val.l);return make_val_error(E_BAD_VALUE, "can't create matrix from ragged array"); }
	sto.val.l[i] = cast_to(f.args[i].val, VAL_ARRAY);
	//check for errors
	if (sto.val.l[i].type == VAL_ERR) {
	    sto = copy_val(sto.val.l[i]);
	    free(sto.val.l);
	    return sto;
	}
    }
    return sto;
}

value make_vec(context* c, cgs_func f) {
    value sto = make_val_undef();
    //just copy the elements
    sto.type = VAL_ARRAY;
    sto.n_els = f.n_args;
    //skip copying an empty list
    if (sto.n_els == 0)
	return sto;
    sto.val.a = (double*)malloc(sizeof(double)*sto.n_els);
    for (size_t i = 0; i < f.n_args; ++i) {
	if (f.args[i].val.type != VAL_NUM) {
	    free(sto.val.a);
	    return make_val_error(E_BAD_TYPE, "cannot cast list with non-numeric types to array");
	}
	sto.val.a[i] = f.args[i].val.val.x;
    }
    return sto;
}

//math functions
static const valtype MATHN_SIG[] = {VAL_NUM};
static const valtype MATHA_SIG[] = {VAL_ARRAY};
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

value make_val_undef() {
    value v;
    v.type = VAL_UNDEF;
    v.n_els = 0;
    v.val.x = 0;
    return v;
}

value make_val_error(parse_ercode code, const char* format, ...) {
    value ret;
    ret.type = VAL_ERR;
    //a nomemory error obviously won't be able to allocate any more memory
    if (code == E_NOMEM) {
	ret.val.e = NULL;
	return ret;
    }
    ret.val.e = (struct error*)malloc(sizeof(struct error));
    if (!ret.val.e)
	return ret;
    ret.val.e->c = code;
    va_list args;
    va_start(args, format);
    vsnprintf(ret.val.e->msg, ERR_BSIZE, format, args);
    va_end(args);
    return ret;
}

value make_val_num(double x) {
    value v;
    v.type = VAL_NUM;
    v.n_els = 1;
    v.val.x = x;
    return v;
}

value make_val_str(const char* s) {
    value v;
    v.type = VAL_STR;
    v.n_els = strlen(s) + 1;
    v.val.s = (char*)malloc(sizeof(char)*v.n_els);
    for (size_t i = 0; i < v.n_els; ++i) v.val.s[i] = s[i];
    return v;
}
value make_val_array(double* vs, size_t n) {
    value v;
    v.type = VAL_ARRAY;
    v.n_els = n;
    v.val.a = (double*)malloc(sizeof(double)*v.n_els);
    memcpy(v.val.a, vs, sizeof(double)*n);
    return v;
}
value make_val_list(const value* vs, size_t n_vs) {
    value v;
    v.type = VAL_LIST;
    v.n_els = n_vs;
    v.val.l = (value*)malloc(sizeof(value)*v.n_els);
    for (size_t i = 0; i < v.n_els; ++i) v.val.l[i] = copy_val(vs[i]);
    return v;
}

value make_val_func(const char* name, size_t n_args, value (*p_exec)(context*, cgs_func)) {
    value ret;
    ret.type = VAL_FUNC;
    ret.n_els = n_args;
    ret.val.f = make_user_func_ex(p_exec);
    return ret;
}
value make_val_inst(context* parent, const char* s) {
    value v;
    v.type = VAL_INST;
    v.val.c = make_context(parent);
    if (s && s[0] != 0) {
	value tmp = make_val_str(s);
	set_value(v.val.c, "__type__", tmp, 0);
    }
    return v;
}

int value_cmp(value a, value b) {
    if (a.type != b.type || a.type == VAL_ERR || b.type == VAL_ERR)
	return 1;
    if (a.type == VAL_NUM) {
	if (a.val.x == b.val.x)
	    return 0;
	if (a.val.x > b.val.x)
	    return 1;
	else
	    return -1;
    } else if (a.type == VAL_STR) {
	//first make sure that both strings are not null while ensuring that null strings compare identically
	if ((a.val.s == NULL || b.val.s == NULL) && a.val.s != b.val.s)
	    return 1;
	return strcmp(a.val.s, b.val.s);
    } else if (a.type == VAL_LIST) {
	//make sure that we can safely iterate
	if ((a.val.l == NULL || b.val.l == NULL) && a.val.l != b.val.l)
	    return 1;
	if (a.n_els > b.n_els)
	    return 1;
	if (a.n_els < b.n_els)
	    return -1;
	for (size_t i = 0; i < a.n_els; ++i) {
	    int tmp = value_cmp(a.val.l[i], b.val.l[i]);
	    if (tmp) return tmp;
	}
	return 0;
    } else if (a.type == VAL_ARRAY) {
	if ((a.val.a == NULL || b.val.a == NULL) && a.val.a != b.val.a)
	    return 1;
	if (a.n_els > b.n_els)
	    return 1;
	if (a.n_els < b.n_els)
	    return -1;
	for (size_t i = 0; i < a.n_els; ++i) {
	    if (a.val.a[i] > b.val.a[i])
		return 1;
	    else if (a.val.a[i] < b.val.a[i])
		return -1;
	}
	return 0;
    }
    //TODO: implement other comparisons
    return -2;
}

int value_str_cmp(value a, const char* b) {
    if (a.type != VAL_STR)
	return 0;
    return strcmp(a.val.s, b);
}

char* rep_string(value v, size_t* n) {
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

value cast_to(value v, valtype t) {
    if (v.type == VAL_UNDEF)
	return make_val_error(E_BAD_TYPE, "cannot cast <undefined> to <%s>", valnames[t]);
    //trivial casts should just be copies
    if (v.type == t)
	return copy_val(v);
    value ret;
    ret.type = t;
    ret.n_els = v.n_els;
    if (t == VAL_LIST) {
	if (v.type == VAL_ARRAY) {
	    ret.val.l = (value*)malloc(sizeof(value)*ret.n_els);
	    for (size_t i = 0; i < ret.n_els; ++i)
		ret.val.l[i] = make_val_num(v.val.a[i]);
	    return ret;
	} else if (v.type == VAL_INST) {
	    //instance -> list
	    ret.n_els = v.val.c->n_memb;
	    ret.val.l = (value*)calloc(ret.n_els, sizeof(value));
	    if (!ret.val.l)
		return make_val_error(E_NOMEM, "");
	    for (size_t i = con_it_next(v.val.c, 0); i < con_size(v.val.c); i = con_it_next(v.val.c, i+1))
		ret.val.l[i] = copy_val(v.val.c->table[i].val);
	    ret.n_els = v.n_els;
	    return ret;
	} else if (v.type == VAL_MAT) {
	    //matrices are basically just an alias for lists
	    ret = copy_val(v);
	    ret.type = t;
	    return ret;
	}
    } else if (t == VAL_MAT) {
	if (v.type == VAL_LIST) {
	    ret.val.l = (value*)malloc(sizeof(value)*ret.n_els);
	    for (size_t i = 0; i < ret.n_els; ++i) {
		//first try making the element an array
		value tmp = cast_to(v.val.l[i], VAL_ARRAY);
		if (tmp.type == VAL_ERR) {
		    //if that doesn't work try making it a matrix
		    cleanup_val(&tmp);
		    tmp = cast_to(v.val.l[i], VAL_MAT);
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
		return make_val_error(E_NOMEM, "");
	    for (size_t i = 0; i < ret.n_els; ++i) {
		if (v.val.l[i].type != VAL_NUM) {
		    free(ret.val.a);
		    return make_val_error(E_BAD_TYPE, "cannot cast list with non-numeric types to array");
		}
		ret.val.a[i] = v.val.l[i].val.x;
	    }
	    return ret;
	}
    } else if (t == VAL_STR) {
	//anything -> string
	ret.val.s = rep_string(v, &(ret.n_els));
    }
    //if we reach this point in execution then there was an error
    ret.type = VAL_UNDEF;
    ret.n_els = 0;
    return ret;
}

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
	for (size_t i = con_it_next(v.val.c, 0); i < (1 << v.val.c->t_bits); i = con_it_next(v.val.c, i+1)) {
	    name_val_pair pair = v.val.c->table[i];
	    print_spaces(f, depth+1);
	    fprintf(f, "%s:", pair.name);
	    //use space economically for simple types
	    if (pair.val.type == VAL_LIST || pair.val.type == VAL_ARRAY || pair.val.type == VAL_INST) {
		fprintf(f, " -V\n");
		print_hierarchy(pair.val, f, depth+1);
	    } else {
		fprintf(f, " ");
		print_hierarchy(pair.val, f, 0);
	    }
	}
	print_spaces(f, depth);
	fprintf(f, /*{*/"}\n");
    }
}

void cleanup_val(value* v) {
    if (v->type == VAL_ERR) {
	free(v->val.e);
    } else if ((v->type == VAL_STR && v->val.s) || (v->type == VAL_ARRAY && v->val.a)) {
	free(v->val.s);
    } else if ((v->type == VAL_LIST || v->type == VAL_MAT) && v->val.l) {
	for (size_t i = 0; i < v->n_els; ++i)
	    cleanup_val(v->val.l + i);
	free(v->val.l);
    } else if (v->type == VAL_ARRAY && v->val.a) {
	free(v->val.a);
    } else if (v->type == VAL_INST && v->val.c) {
	destroy_context(v->val.c);
    } else if (v->type == VAL_FUNC && v->val.f) {
	destroy_user_func(v->val.f);
    }
    v->type = VAL_UNDEF;
    v->val.x = 0;
    v->n_els = 0;
}

value copy_val(const value o) {
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
	    for (size_t i = 0; i < o.n_els; ++i) ret.val.l[i] = copy_val(o.val.l[i]);
	}
    } else if (o.type == VAL_INST) {
	ret.val.c = copy_context(o.val.c);
    } else if (o.type == VAL_FUNC) {
	ret.val.f = copy_user_func(o.val.f);
    } else {
	ret.val.x = o.val.x;
    }
    return ret;
}

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
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "matrix contains type <%s>", valnames[l->val.l[i].type]);
	return 1;
    }
    //move the error to overwrite the list
    value tmp = l->val.l[i];
    for (size_t j = 0; j < l->n_els; ++j) {
	if (i != j)
	    cleanup_val(l->val.l + j);
    }
    free(l->val.l);
    *l = tmp;
    return 1;
}

void val_add(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = make_val_num( (l->val.x)+(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot add arrays of length %lu and %lu", l->n_els, r.n_els);
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
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot add lists of length %lu and %lu", l->n_els, r.n_els);	
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
	l->val.l[l->n_els-1] = copy_val(r);
    } else if (l->type == VAL_STR) {
	size_t l_len, r_len;
	l_len = l->n_els;
	char* r_str = rep_string(r, &r_len);
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
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "cannot add types <%s> and <%s>", valnames[l->type], r.type);
    }
}

void val_sub(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = make_val_num( (l->val.x)-(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot subtract arrays of length %lu and %lu", l->n_els, r.n_els);
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
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot subtract lists of length %lu and %lu", l->n_els, r.n_els);	
	}
	//add a scalar to each element of the array
	for (size_t i = 0; i < l->n_els; ++i) {
	    val_sub(l->val.l+i, r.val.l[i]);
	    if (matrix_err(l, i))
		return;
	}
    } else {
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "cannot subtract types <%s> and <%s>", valnames[l->type], r.type);
    }
}

void val_mul(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = make_val_num( (l->val.x)*(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot multiply arrays of length %lu and %lu", l->n_els, r.n_els);
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
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot multiply lists of length %lu and %lu", l->n_els, r.n_els);	
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
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "cannot multiply types <%s> and <%s>", valnames[l->type], r.type);
    }
}

void val_div(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = make_val_num( (l->val.x)/(r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot divide arrays of length %lu and %lu", l->n_els, r.n_els);
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
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot divide lists of length %lu and %lu", l->n_els, r.n_els);	
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
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "cannot divide types <%s> and <%s>", valnames[l->type], r.type);
    }
}

void val_exp(value* l, value r) {
    if (l->type == VAL_NUM && r.type == VAL_NUM) {
	*l = make_val_num( pow(l->val.x, r.val.x) );
    } else if (l->type == VAL_ARRAY && r.type == VAL_ARRAY) {
	//add the two arrays together
	if (l->n_els != r.n_els) {
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot raise arrays of length %lu and %lu", l->n_els, r.n_els);
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
	    cleanup_val(l);
	    *l = make_val_error(E_OUT_OF_RANGE, "cannot raise matrices of length %lu and %lu", l->n_els, r.n_els);	
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
	cleanup_val(l);
	*l = make_val_error(E_BAD_TYPE, "cannot raise types <%s> and <%s>", valnames[l->type], r.type);
    }
}


/** ======================================================== cgs_func ======================================================== **/

void cleanup_func(cgs_func* f) {
    if (f) {
	for (size_t i = 0; i < f->n_args; ++i)
	    cleanup_name_val_pair(f->args[i]);
    }
}
cgs_func copy_func(const cgs_func o) {
    cgs_func f;f.name = NULL;
    if (o.name) f.name = strdup(o.name);
    f.n_args = o.n_args;
    for (size_t i = 0; i < f.n_args; ++i) {
	f.args[i].val = copy_val(o.args[i].val);
	f.args[i].name = (o.args[i].name)? strdup(o.args[i].name) : NULL;
    }
    return f;
}
void swap_func(cgs_func* a, cgs_func* b) {
    char* tmp_name = a->name;
    a->name = b->name;
    b->name = tmp_name;
    size_t tmp_n_args = a->n_args;
    a->n_args = b->n_args;
    b->n_args = tmp_n_args;
    //WLOG fix sf and lf to be the pointers to the functions smaller and larger number of arguments respectively
    cgs_func* sf = a;
    cgs_func* lf = b;
    size_t min_n_args = a->n_args;
    size_t max_n_args = b->n_args;
    if (b->n_args < a->n_args) {
	sf = b;
	lf = a;
	min_n_args = b->n_args;
	max_n_args = a->n_args;
    }
    //finally we can actually swap each element, a full swap only needs to be done for the minimal number of arguments
    size_t i = 0;
    for (; i < min_n_args; ++i) {
	name_val_pair tmp = sf->args[i];
	sf->args[i] = lf->args[i];
	lf->args[i] = tmp;
    }
    for (; i < max_n_args; ++i) {
	sf->args[i] = lf->args[i];
	sf->args[i] = lf->args[i];
    }
}

/** ======================================================== name_val_pair ======================================================== **/

name_val_pair make_name_val_pair(const char* p_name, value p_val) {
    name_val_pair ret;
    ret.name = (p_name) ? strdup(p_name) : NULL;
    ret.val = copy_val(p_val);
    return ret;
}
void cleanup_name_val_pair(name_val_pair nv) {
    if (nv.name)
	free(nv.name);
    if (nv.val.type != VAL_UNDEF)
	cleanup_val(&nv.val);
}

/** ============================ context ============================ **/

//non-cryptographically hash the string str
static inline size_t fnv_1(const char* str, unsigned char t_bits) {
    if (str == NULL)
	return 0;
    size_t ret = FNV_OFFSET;
    for (size_t i = 0; str[i]; ++i) {
	ret = ret^str[i];
	ret = ret*FNV_PRIME;
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
 * c: the context to look in
 * name: the name to look for
 * ind: the location where we find the matching index
 * returns: 1 if a match was found, 0 otherwise
 */
static inline int find_ind(const struct context* c, const char* name, size_t* ind) {
    size_t ii = fnv_1(name, c->t_bits);
    size_t i = ii;
    while (c->table[i].name) {
	if (strcmp(name, c->table[i].name) == 0) {
	    *ind = i;
	    return 1;
	}
	//i = (i+1) if i+1 < table_size or 0 otherwise (note table_size == 1<<c->t_bits)
	i = (i+1) & ((1 << c->t_bits)-1);
	if (i == ii)
	    break;
    }
    *ind = i;
    return 0;
}

/**
 * Grow the context if necessary
 * returns: 1 if growth was performed
 */
static inline int grow_context(struct context* c) {
    if (c->n_memb*GROW_LOAD_DEN > (1 << c->t_bits)*GROW_LOAD_NUM) {
	//create a new context with twice as many elements
	struct context nc;
	nc.parent = c->parent;
	nc.t_bits = c->t_bits + 1;
	nc.n_memb = 0;
	nc.table = (name_val_pair*)calloc((1 << nc.t_bits), sizeof(name_val_pair));
	//make sure allocation was successful
	if (!nc.table) {
	    printf("Fatal error: ran out of memory!\n");
	    exit(1);
	}
	//we have to rehash every member in the old table
	for (size_t i = 0; i < (1 << c->t_bits); ++i) {
	    if (c->table[i].name == NULL)
		continue;
	    //only move non-null members
	    size_t new_ind;
	    if (!find_ind(&nc, c->table[i].name, &new_ind))
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



struct context* make_context(context* parent) {
    context* c = (context*)malloc(sizeof(context));
    if (!c) return NULL;
    c->parent = parent;
    c->n_memb = 0;
    c->t_bits = DEF_TAB_BITS;
    //double the allocated size for root contexts (since they're likely to hold more stuff)
    if (!parent) c->t_bits++;
    c->table = (name_val_pair*)calloc(1 << c->t_bits, sizeof(name_val_pair));
    if (!c->table) {
	free(c);
	return NULL;
    }
    if (!parent)
	setup_builtins(c);
    return c;
}

struct context* copy_context(const context* o) {
    if (!o) return NULL;
    context* c = (context*)malloc(sizeof(context));
    if (!c) return NULL;
    c->table = (name_val_pair*)calloc(con_size(o), sizeof(name_val_pair));
    if (!c->table) {
	free(c);
	return NULL;
    }
    c->parent = o->parent;
    c->n_memb = o->n_memb;
    c->t_bits = o->t_bits;
    for (size_t i = con_it_next(o, 0); i < con_size(o); i = con_it_next(o, i+1)) {
	c->table[i].name = strdup(o->table[i].name);
	c->table[i].val = copy_val(o->table[i].val);
    }
    return c;
}

void destroy_context(struct context* c) {
    //erase the hash table
    for (size_t i = con_it_next(c, 0); i < con_size(c); i = con_it_next(c,i+1))
	cleanup_name_val_pair(c->table[i]);
    free(c->table);
    free(c);
}
value do_op(struct context* c, char* str, size_t i) {
    value sto = make_val_undef();
    //some operators (==, >=, <=) take up more than one character, test for these
    int op_width = 1;
    if (str[i+1] == '=')
	op_width = 2;
    //Store the operation before setting it to zero
    char term_char = str[i];
    str[i] = 0;
    //ternary operators and dereferences are special cases
    if (term_char == '?') {
	//the colon must be present
	char* col_ind = strchr_block(str+i+1, ':');
	if (!col_ind)
	    return make_val_error(E_BAD_SYNTAX, "expected : in ternary");
	value l = parse_value(c, str);
	if (l.type == VAL_ERR)
	    return l;
	//0 branch
	if (l.type == VAL_UNDEF || l.val.x == 0) {
	    sto = parse_value(c, col_ind+1);
	    return sto;
	} else {
	    //1 branch
	    *col_ind = 0;
	    sto = parse_value(c, str+i+op_width);
	    *col_ind = ':';
	    return sto;
	}
    } else if (term_char == '.') {
	value inst_val = lookup(c, find_token_before(str, i));
	if (inst_val.type != VAL_INST)
	    return make_val_error(E_BAD_TYPE, "Error: tried to lookup from non instance type\n");
	str[i] = term_char;
	return parse_value(inst_val.val.c, str+i+1);
    }

    //parse right and left values
    value l = parse_value(c, str);
    if (l.type == VAL_ERR)
	return l;
    value r = parse_value(c, str+i+op_width);
    if (r.type == VAL_ERR) {
	cleanup_val(&l);
	return r;
    }
    //WLOG fix all array operations to have the array first
    if (l.type == VAL_NUM && r.type == VAL_ARRAY)
	swap_val(&l, &r);
    sto.type = VAL_NUM;
    sto.n_els = 1;
    //handle equality comparisons
    if (term_char == '=' && str[i+1] == '=') {
	sto.val.x = (value_cmp(l, r) == 0);
    } else if (term_char == '>') {
	int cmp = value_cmp(l, r);
	if (cmp < -1) {
	    sto = make_val_error(E_BAD_VALUE, "cannot compare types <%s> and <%s>", valnames[l.type], valnames[r.type]);
	    goto finish;
	}
	if (str[i+1] == '=')
	    sto.val.x = (cmp >= 0);
	else
	    sto.val.x = (cmp > 0);
    } else if (term_char == '<') {
	int cmp = value_cmp(l, r);
	if (cmp < -1) {
	    sto = make_val_error(E_BAD_VALUE, "cannot compare types <%s> and <%s>", valnames[l.type], valnames[r.type]);
	    goto finish;
	}
	if (str[i+1] == '=')
	    sto.val.x = (cmp <= 0);
	else
	    sto.val.x = (cmp < 0);
    //handle arithmetic
    } else if (term_char == '+' && (i == 0 || str[i-1] != 'e')) {
        val_add(&l, r);
	goto finish_arith;
    } else if (term_char == '-') {
        val_sub(&l, r);
	goto finish_arith;
    } else if (term_char == '*') {
	if (str[i+1] == '*')
	    val_exp(&l, r);
	else
	    val_mul(&l, r);
	goto finish_arith;
    } else if (term_char == '/') {
	val_div(&l, r);
	goto finish_arith;
    }
finish:
    str[i] = term_char;
    cleanup_val(&l);
    cleanup_val(&r);
    return sto;
finish_arith:
    str[i] = term_char;
    cleanup_val(&r);
    return l;
}

void setup_builtins(struct context* c) {
    //create builtins
    set_value(c, "false",	make_val_num(0), 0);
    set_value(c, "true",	make_val_num(1), 0);//create horrible (if amusing bugs when someone tries to assign to true or false
    set_value(c, "typeof", 	make_val_func("typeof", 1, &get_type), 0);
    set_value(c, "range", 	make_val_func("range", 1, &make_range), 0);
    set_value(c, "linspace", 	make_val_func("linspace", 3, &make_linspace), 0);
    set_value(c, "flatten", 	make_val_func("flatten", 1, &flatten_list), 0);
    set_value(c, "array", 	make_val_func("array", 1, &make_array), 0);
    set_value(c, "vec", 	make_val_func("vec", 1, &make_vec), 0);
    set_value(c, "cat", 	make_val_func("cat", 1, &concatenate), 0);
    set_value(c, "print", 	make_val_func("print", 1, &print), 0);
    //math stuff
    value tmp = make_val_inst(c, "math");
    context* math_c = tmp.val.c;
    set_value(math_c, "pi", 	make_val_num(M_PI), 0);
    set_value(math_c, "e", 	make_val_num(M_E), 0);
    set_value(math_c, "sin", 	make_val_func("sin", 1, &fun_sin), 0);
    set_value(math_c, "cos", 	make_val_func("cos", 1, &fun_cos), 0);
    set_value(math_c, "tan", 	make_val_func("tan", 1, &fun_tan), 0);
    set_value(math_c, "exp", 	make_val_func("exp", 1, &fun_exp), 0);
    set_value(math_c, "arcsin", make_val_func("arcsin", 1, &fun_asin), 0);
    set_value(math_c, "arccos", make_val_func("arccos", 1, &fun_acos), 0);
    set_value(math_c, "arctan", make_val_func("arctan", 1, &fun_atan), 0);
    set_value(math_c, "log", 	make_val_func("log", 1, &fun_log), 0);
    set_value(math_c, "sqrt", 	make_val_func("sqrt", 1, &fun_sqrt), 0);
    set_value(c, "math", tmp, 0);
}

value lookup(const struct context* c, const char* str) {
    size_t i;
    if (find_ind(c, str, &i))
	return c->table[i].val;
    //try searching through the parent if that didn't work
    if (c->parent)
	return lookup(c->parent, str);
    //reaching this point in execution means the matching entry wasn't found
    return make_val_undef();
}

int lookup_object(const context* c, const char* str, const char* typename, context** sto) {
    value vobj = lookup(c, str);
    if (vobj.type != VAL_INST)
	return -1;
    value tmp = lookup(vobj.val.c, "__type__");
    if (tmp.type != VAL_STR || strcmp(tmp.val.s, typename))
	return -2;
    if (sto) *sto = vobj.val.c;
    return 0;
}

int lookup_c_array(const context* c, const char* str, double* sto, size_t n) {
    if (sto == NULL || n == 0)
	return 0;
    value tmp = lookup(c, str);
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

int lookup_c_str(const context* c, const char* str, char* sto, size_t n) {
    if (sto == NULL || n == 0)
	return 0;
    value tmp = lookup(c, str);
    if (tmp.type != VAL_STR)
	return -1;
    //bounds check
    size_t n_write = (tmp.n_els > n) ? n : tmp.n_els;
    memcpy(sto, tmp.val.s, sizeof(char)*n_write);
    return 0;
}

int lookup_int(const context* c, const char* str, int* sto) {
    value tmp = lookup(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = (int)tmp.val.x;
    return 0;
}
int lookup_size(const context* c, const char* str, size_t* sto) {
    value tmp = lookup(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = (size_t)tmp.val.x;
    return 0;
}
int lookup_float(const context* c, const char* str, double* sto) {
    value tmp = lookup(c, str);
    if (tmp.type == VAL_NUM)
	return -1;
    if (sto) *sto = tmp.val.x;
    return 0;
}

void set_value(struct context* c, const char* p_name, value p_val, int copy) {
    //generate a fake name if none was provided
    if (!p_name || p_name[0] == 0) {
	char tmp[BUF_SIZE];
	snprintf(tmp, BUF_SIZE, "\e_%lu", c->n_memb);
	return set_value(c, tmp, p_val, copy);
    }
    size_t ti = fnv_1(p_name, c->t_bits);
    if (!find_ind(c, p_name, &ti)) {
	//if there isn't already an element with that name we have to expand the table and add a member
	if (grow_context(c))
	    find_ind(c, p_name, &ti);
	c->table[ti].name = strdup(p_name);
	c->table[ti].val = (copy)? copy_val(p_val) : p_val;
	++c->n_memb;
    } else {
	//otherwise we need to cleanup the old value and add the new
	cleanup_val( &(c->table[ti].val) );
	c->table[ti].val = (copy)? copy_val(p_val) : p_val;
    }
}

struct for_state {
    char* expr_name;
    char* var_name;
    char* for_start;
    char* in_start;
    size_t expr_len;
    value it_list;
    name_val_pair prev;
    size_t var_ind;
};

static inline struct for_state make_for_state(context* c, const char* start, char* for_start, value* er) {
    struct for_state fs;
    size_t n;
    //now look for a block labeled "in"
    fs.for_start = for_start;
    fs.in_start = token_block(for_start+KEY_FOR_LEN, "in");
    if (!fs.in_start) {
	*er = make_val_error(E_BAD_SYNTAX, "expected keyword in");
	return fs;
    }
    //the expression is the content before the for statement
    fs.expr_len = for_start-(start+1);
    //the variable name is whatever is in between the "for" and the "in"
    fs.in_start[0] = 0;
    fs.var_name = trim_whitespace(for_start+KEY_FOR_LEN, &n);
    fs.var_name = strndup(fs.var_name, n+1);
    for_start[KEY_FOR_LEN+1+n] = ' ';//reset the for string from trim_whitespace
    //now parse the list we iterate over
    char* list_expr = strdup(fs.in_start+KEY_IN_LEN);
    fs.it_list = parse_value(c, list_expr);
    free(list_expr);
    if (fs.it_list.type == VAL_ERR) {
	*er = make_val_error(E_BAD_SYNTAX, "in expression %s", fs.in_start+KEY_IN_LEN);
	free(fs.var_name);
	return fs;
    }
    if (fs.it_list.type != VAL_ARRAY && fs.it_list.type != VAL_LIST) {
	*er =  make_val_error(E_BAD_TYPE, "can't iterate over type <%s>", valnames[fs.it_list.type]);
	free(fs.var_name);
	return fs;
    }
    //the prototype expression needs to be null terminated
    for_start[0] = 0;
    //we need to add a variable with the appropriate name to loop over. We write a value and save the value there before so we can remove it when we're done
    find_ind(c, fs.var_name, &(fs.var_ind));
    fs.prev = c->table[fs.var_ind];
    c->table[fs.var_ind].name = fs.var_name;
    fs.expr_name = strndup(start+1, fs.expr_len);
    *er = make_val_undef();
    return fs;
}

static inline void finish_for_state(context* c, struct for_state fs) {
    //we need to reset the table with the loop index before iteration
    cleanup_name_val_pair(c->table[fs.var_ind]);
    c->table[fs.var_ind] = fs.prev;
    //reset the string so that it can be parsed again
    fs.for_start[0] = 'f';
    fs.in_start[0] = 'i';
    //free the memory from the iteration list
    cleanup_val(&fs.it_list);
    free(fs.expr_name);
}

/**
 * Read a string of the format [x, y, z] into an Eigen::Vector3.
 * returns: 0 on success or an error code
 * 	-1: insufficient tokens
 * 	-2: one of the tokens supplied was invalid
 */
static inline value parse_list(struct context* c, char* str) {
    value sto;
    //find the start and the end of the vector
    char* start = strchr(str, '[');
    if (!start)
	return make_val_error(E_BAD_SYNTAX, "expected [");
    //make sure that the string is null terminated
    char* end = strchr_block(start+1, ']');
    if (!end)
	return make_val_error(E_BAD_SYNTAX, "expected ]");
    *end = 0;

    //read the coordinates separated by spaces
    value* lbuf;
    //check if this is a list interpretation
    char* for_start = token_block(start+1, "for");
    if (for_start) {
	struct for_state fs = make_for_state(c, start, for_start, &sto);
	if (sto.type == VAL_ERR)
	    return sto;
	//setup a buffer to hold the list
	sto.n_els = fs.it_list.n_els;
	lbuf = (value*)calloc(sto.n_els, sizeof(value));
	//we now iterate through the list specified, substituting VAL in the expression with the current value
	for (size_t i = 0; i < sto.n_els; ++i) {
	    if (fs.it_list.type == VAL_LIST)
		c->table[fs.var_ind].val = fs.it_list.val.l[i];
	    else if (fs.it_list.type == VAL_ARRAY)
		c->table[fs.var_ind].val = make_val_num(fs.it_list.val.a[i]);
	    //we have to copy the expression since it might have been destroyed on the last iteration
	    strncpy(fs.expr_name, start+1, fs.expr_len);
	    lbuf[i] = parse_value(c, fs.expr_name);
	    if (lbuf[i].type == VAL_ERR) {
		value ret = copy_val(lbuf[i]);
		for (size_t j = 0; j < i; ++j)
		    cleanup_val(lbuf+j);
		free(lbuf);
		finish_for_state(c, fs);
		return ret;
	    }
	}
	finish_for_state(c, fs);
    } else {
	char** list_els = csv_to_list(start+1, ',', &sto.n_els);
	if (!list_els)
	    return make_val_error(E_BAD_SYNTAX, "couldn't parse list string");
	lbuf = (value*)calloc(sto.n_els, sizeof(value));
	for (size_t i = 0; list_els[i] && i < sto.n_els; ++i) {
	    lbuf[i] = parse_value(c, list_els[i]);
	    if (lbuf[i].type == VAL_ERR) {
		free(list_els);
		sto = copy_val(lbuf[i]);
		free(lbuf);
		return sto;
	    }
	}
	free(list_els);
    }
    //cleanup and reset the string
    *end = ']';
    //set number of elements and type
    sto.type = VAL_LIST;
    sto.val.l = lbuf;
    return sto;
}

cgs_func parse_func(struct context* c, char* token, long open_par_ind, value* v_er, char** end, int name_only) {
    struct cgs_func f;

    //by default we want to indicate that we didn't get to the end
    if (end) *end = NULL;
    f.n_args = 0;
    //infer the location of the open paren index if the user didn't specify it
    if (open_par_ind < 0 || token[open_par_ind] != '('/*)*/) {
	char* par_char = strchr(token, '('/*)*/);
	//make sure there actually is an open paren
	if (!par_char) {
	    *v_er = make_val_error(E_BAD_SYNTAX, "expected ("/*)*/);
	    return f;
	}
	open_par_ind = par_char - token;
    }

    //break the string up at the parenthesis and remove surrounding whitespace
    token[open_par_ind] = 0;
    f.name = trim_whitespace(token, NULL);

    //now remove whitespace from the ends of the string
    char* arg_str = token+open_par_ind+1;
    //make sure that the string is null terminated
    char* term_ptr = strchr_block(arg_str, /*(*/')');
    if (!term_ptr) {
	*v_er = make_val_error(E_BAD_SYNTAX, /*(*/"expected )");
	return f;
    }
    *term_ptr = 0;
    if (end) *end = term_ptr+1;
 
    //read the arguments separated by spaces
    char** list_els = csv_to_list(arg_str, ',', &(f.n_args));
    if (!list_els) {
	*v_er = make_val_error(E_BAD_SYNTAX, "couldn't parse function signature");
	return f;
    }
    //make sure that we don't go out of bounds
    if (f.n_args >= ARGS_BUF_SIZE) {
	*v_er = make_val_error(E_OUT_OF_RANGE, "too many arguments");
	free(list_els);
	return f;
    }
    if (name_only) {
	for (size_t i = 0; list_els[i] && i < f.n_args; ++i) {
	    f.args[i].name = strdup(list_els[i]);
	    f.args[i].val = make_val_undef();
	}
    } else {
	for (size_t i = 0; list_els[i] && i < f.n_args; ++i) {
	    //handle named arguments
	    char* eq_loc = strchr_block(list_els[i], '=');
	    if (eq_loc) {
		*eq_loc = 0;
		f.args[i].name = strdup(list_els[i]);
		list_els[i] = eq_loc+1;
	    } else {
		f.args[i].name = NULL;
	    }
	    f.args[i].val = parse_value(c, list_els[i]);
	}
    }
    //cleanup and reset the string
    free(list_els);
    return f;
}

/**
 * Get the location of the first operator which is not enclosed in a block expression
 * op_loc: store the location of the operator
 * open_ind: store the location of the first enclosing block
 * close_ind: store the location of the last escaping block
 * returns: the index of the first expression or 0 in the event of an error (it is impossible for a valid expression to have an operator as the first character)
 */
static inline value find_operator(char* str, size_t* op_loc, int* first_open_ind, int* last_close_ind) {
    *op_loc = 0;
    *first_open_ind = -1;*last_close_ind = -1;
    //keeps track of open and close [], (), {}, and ""
    stack(blk_type) blk_stk = make_stack(blk_type)();
    blk_type start_ind;
    //variable names are not allowed to start with '+', '-', or a digit and may not contain any '.' symbols. Use this to check whether the value is numeric
    int is_numeric = (str[0] == '+' || str[0] == '-' || str[0] == '.' || (str[0] >= '0' && str[0] <= '9'));

    //keep track of the precedence of the orders of operation (lower means executed later) ">,=,>=,==,<=,<"=4 "+,-"=3, "*,/"=2, "**"=1
    char op_prec = 0;
    for (_uint i = 0; str[i] != 0; ++i) {
	if (str[i] == '['/*]*/) {
	    push(blk_type)(&blk_stk, BLK_SQUARE);
	    if (*first_open_ind == -1) { *first_open_ind = i; }
	} else if (str[i] == /*[*/']') {
	    if (pop(blk_type)(&blk_stk, &start_ind) || start_ind != BLK_SQUARE) {
		destroy_stack(blk_type)(&blk_stk, NULL);
		return make_val_error(E_BAD_SYNTAX, /*[*/"unexpected ]");
	    }
	    *last_close_ind = i;
	} else if (str[i] == '('/*)*/) {
	    //keep track of open and close parenthesis, these will come in handy later
	    push(blk_type)(&blk_stk, BLK_PAREN);
	    //only set the open index if this is the first match
	    if (*first_open_ind == -1) { *first_open_ind = i; }
	} else if (str[i] == /*(*/')') {
	    if (pop(blk_type)(&blk_stk, &start_ind) || start_ind != BLK_PAREN) {
		destroy_stack(blk_type)(&blk_stk, NULL);
		return make_val_error(E_BAD_SYNTAX, /*(*/"unexpected )");
	    }
	    *last_close_ind = i;
	} else if (str[i] == '{'/*}*/) {
	    //keep track of open and close parenthesis, these will come in handy later
	    push(blk_type)(&blk_stk, BLK_CURLY);
	    //only set the open index if this is the first match
	    if (*first_open_ind == -1) { *first_open_ind = i; }
	} else if (str[i] == /*{*/'}') {
	    if (pop(blk_type)(&blk_stk, &start_ind) || start_ind != BLK_CURLY) {
		destroy_stack(blk_type)(&blk_stk, NULL);
		return make_val_error(E_BAD_SYNTAX, /*{*/"unexpected }");
	    }
	    //only set the end paren location if it hasn't been set yet and the stack has no more parenthesis to remove
	    *last_close_ind = i;
	} else if (str[i] == '\"' && (i == 0 || str[i-1] != '\\')) {
	    int code = peek(blk_type)(&blk_stk, 1, &start_ind);
	    if (code || start_ind != BLK_QUOTE) {
		//quotes need to be handled in a special way
		push(blk_type)(&blk_stk, BLK_QUOTE);
		//only set the open index if this is the first match
		if (*first_open_ind == -1) *first_open_ind = i;
	    } else {
		pop(blk_type)(&blk_stk, &start_ind);
		*last_close_ind = i;
	    }
	} else if (str[i] == '/') {
	    //ignore everything after a comment
	    if (str[i+1] == '/')
		break;
	}

	if (blk_stk.ptr == 0) {
	    //check if we found a comparison operation symbol
	    if (((str[i] == '=' && str[i+1] == '=') || str[i] == '>' || str[i] == '<') && op_prec < 5) {
		op_prec = 5;
		*op_loc = i;
	    } else if (i != 0 && (str[i] == '+' || str[i] == '-') && str[i-1] != 'e' && op_prec < 4) {
		//remember to recurse after we finish looping
		op_prec = 4;
		*op_loc = i;
	    } else if (str[i] == '^' && op_prec < 3) {
		op_prec = 3;
		*op_loc = i;
	    } else if ((str[i] == '*' || str[i] == '/') && op_prec < 2) {
		op_prec = 2;
		*op_loc = i;
	    } else if (op_prec < 1 && (str[i] == '?' || (str[i] == '.' && !is_numeric))) {
		op_prec = 1;
		*op_loc = i;
	    }
	}
    }
    if (blk_stk.ptr > 0) {
	pop(blk_type)(&blk_stk, &start_ind);
	value er;
	switch (start_ind) {
	    case BLK_SQUARE: er = make_val_error(E_BAD_SYNTAX, /*[*/"expected ]");break;
	    case BLK_PAREN: er = make_val_error(E_BAD_SYNTAX, /*(*/"expected )");break;
	    case BLK_CURLY: er = make_val_error(E_BAD_SYNTAX, /*{*/"expected }");break;
	    case BLK_QUOTE: er = make_val_error(E_BAD_SYNTAX, "unexpected \"");break;
	    default: er = make_val_error(E_BAD_SYNTAX, "unrecognized syntax error");break;
	}
	destroy_stack(blk_type)(&blk_stk, NULL);
	return er;
    }
    destroy_stack(blk_type)(&blk_stk, NULL);
    return make_val_undef();
}


// helper for parse_value to handle numeric literals
static inline value parse_literal_string(char* str, int first_open_ind, int last_close_ind) {
    value sto;
    //this is a string
    sto.type = VAL_STR;
    sto.n_els = last_close_ind-first_open_ind;
    //allocate memory and copy
    sto.val.s = (char*)malloc(sizeof(char)*sto.n_els);
    for (size_t j = 0; j < sto.n_els-1; ++j)
	sto.val.s[j] = str[first_open_ind+j+1];
    sto.val.s[sto.n_els-1] = 0;
    return sto;
}

//helper for parse_value to hand list literals
static inline value parse_literal_list(struct context* c, char* str, int first_open_ind, int last_close_ind) {
    //first check to see if the user is trying to access an element
    str[first_open_ind] = 0;
    char* pre_list_name = find_token_before(str, first_open_ind);
    //if the string is empty then we're creating a new list, otherwise we're accessing an existing list
    if (pre_list_name[0] == 0) {
	str[first_open_ind] = '[';//]
	return parse_list(c, str+first_open_ind);
    } else {
	//check if this is actually dereferencing from a list. this technically isn't a literal so I lied
	value lst = lookup(c, pre_list_name);
	str[first_open_ind] = '[';//]
	if (lst.type != VAL_LIST && lst.type != VAL_ARRAY && lst.type != VAL_MAT) {
	    cleanup_val(&lst);
	    return make_val_error(E_BAD_TYPE, "tried to index from non list type %s", valnames[lst.type]);
	}
	str[last_close_ind] = 0;
	value contents = parse_value(c, str+first_open_ind+1);
	str[last_close_ind] = /*[*/']';
	//check that we found the list and that it was valid
	if (contents.type != VAL_NUM) {
	    cleanup_val(&contents);
	    return make_val_error(E_BAD_SYNTAX, "only integers are valid indices\n");
	}
	//now figure out the index
	long tmp = (long)(contents.val.x);
	if (tmp < 0) tmp = lst.n_els+tmp;
	if (tmp < 0)
	    return make_val_error(E_OUT_OF_RANGE, "index %d is out of range for list of size %lu.\n", tmp, lst.n_els);
	size_t ind = (size_t)tmp;
	if (ind >= lst.n_els)
	    return make_val_error(E_OUT_OF_RANGE, "index %d is out of range for list of size %lu.\n", tmp, lst.n_els);
	if (lst.type == VAL_LIST || lst.type == VAL_MAT)
	    return copy_val(lst.val.l[ind]);
	else if (lst.type == VAL_ARRAY)
	    return make_val_num(lst.val.a[ind]);
    }
    return make_val_error(E_BAD_SYNTAX, "couldn't parse list expression %s",  str);
}

//parse table declaration statements
static inline value parse_literal_table(struct context* c, char* str, int first_open_ind, int last_close_ind) {
    //now parse the argument as a context
    size_t n_els;
    str[last_close_ind] = 0;
    char** list_els = csv_to_list(str+first_open_ind+1, ',', &n_els);
    if (!list_els)
	return make_val_error(E_BAD_SYNTAX, "couldn't parse list");
    //setup the context
    value sto;
    sto.val.c = make_context(c);
    sto.n_els = n_els;
    //insert context members
    for (size_t j = 0; list_els[j] && j < n_els; ++j) {
	char* cur_name = NULL;
	char* rval = list_els[j];
	char* eq_loc = strchr_block(list_els[j], '=');
	if (eq_loc) {
	    *eq_loc = 0;
	    cur_name = trim_whitespace(list_els[j], NULL);
	    rval = eq_loc+1;
	}
	value tmp = parse_value(sto.val.c, rval);
	if (tmp.type == VAL_ERR) {
	    free(list_els);
	    return tmp;
	}
	set_value(sto.val.c, cur_name, tmp, 1);
	cleanup_val(&tmp);
    }
    free(list_els);
    sto.type = VAL_INST;
    return sto;
}

//parse function definition/call statements
static inline value parse_literal_func(struct context* c, char* str, int first_open_ind, int last_close_ind) {
    value sto = make_val_undef();
    //check to see if this is a function call (it is if there are any non-whitespace characters before the open paren
    for (size_t j = 0; j < first_open_ind && str[j]; ++j) {
	if (is_whitespace(str[j]))
	    continue;
	cgs_func f;
	//check to see if this is a function declaration, this must be handled differently
	if (strncmp(str+j, "fun", KEY_DEF_LEN) == 0 &&
	(is_whitespace(str[j+KEY_DEF_LEN]) || str[j+KEY_DEF_LEN] == '('/*)*/)) {
	    char* f_end;
	    f = parse_func(c, str, first_open_ind, &sto, &f_end, 1);
	    if (sto.type == VAL_ERR)
		return sto;
	    //find the contents in the curly brace and separate by semicolons
	    line_buffer* b = make_line_buffer_sep(f_end, ';');
	    sto.type = VAL_FUNC;
	    sto.n_els = f.n_args;
	    line_buffer_ind start, end;
	    start = make_lbi(0, j);
	    sto.val.f = make_user_func_lb(f, lb_get_enclosed(b, start, &end, '{', '}', 0, 0));
	    cleanup_func(&f);
	    destroy_line_buffer(b);
	    return sto;
	} else {
	    //we can't leave this as zero in case the user needs to do some more operations
	    str[last_close_ind+1] = 0;
	    f = parse_func(c, str, first_open_ind, &sto, NULL, 0);
	    if (sto.type == VAL_ERR)
		return sto;
	    //otherwise lookup the function
	    value func_val = lookup(c, f.name);
	    if (func_val.type == VAL_FUNC) {
		//make sure that the function was found and that sufficient arguments were provided
		if (func_val.n_els <= f.n_args) {
		    sto = uf_eval(func_val.val.f, c, f);
		} else {
		    str[first_open_ind] = 0;
		    sto = make_val_error(E_LACK_TOKENS, "Error: unrecognized function name %s\n", f.name);
		    str[first_open_ind] = '(';//)
		}
	    } else {
		str[first_open_ind] = 0;
		sto = make_val_error(E_BAD_TYPE, "Error: unrecognized function name %s\n", f.name);
		str[first_open_ind] = '(';//);
	    }
	    cleanup_func(&f);
	    return sto;
	}
    }
    //otherwise interpret this as a parenthetical expression
    str[last_close_ind] = 0;
    size_t reset_ind;
    char* tmp_str = trim_whitespace(str+first_open_ind+1, &reset_ind);
    sto = parse_value(c, tmp_str);
    tmp_str[reset_ind] = ' ';
    str[first_open_ind] = '(';
    str[last_close_ind] = ')';
    return sto;
}

value parse_value(struct context* c, char* str) {
    value sto = make_val_undef();
    //iterate until we hit a non whitespace character
    while (*str && is_whitespace(*str)) ++str;
    if (str[0] == 0)
	return sto;

    //store locations of the first instance of different operators. We do this so we can quickly look up new operators if we didn't find any other operators of a lower precedence (such operators are placed in the tree first).
    int first_open_ind, last_close_ind;
    //store the location of the operator
    size_t op_loc;
    sto = find_operator(str, &op_loc, &first_open_ind, &last_close_ind);
    if (sto.type == VAL_ERR)
	return sto;

    //last try removing parenthesis 
    if (op_loc == 0) {
	size_t reset_ind = 0;
	//if there isn't a valid parenthetical expression, then we should interpret this as a variable
	if (first_open_ind < 0 || last_close_ind < 0) {
	    str = trim_whitespace(str, &reset_ind);
	    //ensure that empty strings return undefined
	    if (str[0] == 0)
		return make_val_undef();
	    sto = lookup(c, str);
	    if (sto.type == VAL_UNDEF) {
		//try interpreting as a number
		errno = 0;
		sto.val.x = strtod(str, NULL);
		sto.n_els = 1;
		if (errno)
		    return make_val_error(E_UNDEF, "undefined token %s", str);
		sto.type = VAL_NUM;
	    }
	    sto = copy_val(sto);
	    str[reset_ind] = ' ';//all whitespace is treated identically so it doesn't matter
	} else {
	    //if there are enclosed blocks then we need to read those
	    switch (str[first_open_ind]) {
	    case '\"': if (str[last_close_ind] != '\"') return make_val_error(E_BAD_SYNTAX, "expected \"");
		return parse_literal_string(str, first_open_ind, last_close_ind);break;
	    case '[': if (str[last_close_ind] != ']') return make_val_error(E_BAD_SYNTAX, /*[*/"expected ]");
		return parse_literal_list(c, str, first_open_ind, last_close_ind);break;
	    case '{': if (str[last_close_ind] != '}') return make_val_error(E_BAD_SYNTAX, /*{*/"expected }");
		return parse_literal_table(c, str, first_open_ind, last_close_ind);break;
	    case '(': if (str[last_close_ind] != ')') return make_val_error(E_BAD_SYNTAX, /*(*/"expected )");
		return parse_literal_func(c, str, first_open_ind, last_close_ind);break;
	    }
	}
    } else {
	return do_op(c, str, op_loc);
    }
    return sto;
}

static inline read_state make_read_state(const line_buffer* pb) {
    read_state ret;
    ret.pos = make_lbi(0,0);
    ret.b = pb;
    ret.buf_size = BUF_SIZE;
    ret.buf = (char*)calloc(ret.buf_size, sizeof(char));
    return ret;
}
static inline void destroy_read_state(read_state rs) {
    free(rs.buf);
}

/**helper function for read_from lines that reads a single line
 */
static inline value read_single_line(context* c, read_state* rs) {
    //tracking variables
    size_t len = lb_get_line_size(rs->b, rs->pos.line);
    size_t k = 0;
    int started = 0;
    char* lval = NULL;
    size_t rval_ind = 0;
    line_buffer_ind init = rs->pos;
    for (rs->pos.off = 0;; ++rs->pos.off) {
	//make sure the buffer is large enough
	if (k >= rs->buf_size) {
	    rs->buf_size *= 2;
	    char* tmp = realloc(rs->buf, sizeof(char)*rs->buf_size);
	    if (!tmp) return make_val_error(E_NOMEM, "");
	    rs->buf = tmp;
	}
	//exit the loop when we reach the end, but make sure to include whatever parts haven't already been included
	if (rs->pos.off >= len || lb_get(rs->b, rs->pos) == 0) {
	    rs->buf[k] = 0;
	    break;
	}
	//ignore preceeding whitespace
	if (lb_get(rs->b, rs->pos) != ' ' || lb_get(rs->b, rs->pos) != '\t' || lb_get(rs->b, rs->pos) != '\n')
	    started = 1;
	if (started) {
	    //handle comments
	    if (lb_get(rs->b, rs->pos) == '/' && rs->pos.off > 0 && lb_get(rs->b, lbi_sub(rs->pos, 1)) == '/') {
		//we don't care about lines that only contain comments, so we should skip over them, but in the other event we need to skip to the end of the line
		if (k == 1)
		    started = 0;
		else
		    rs->pos.off = lb_get_line_size(rs->b, rs->pos.line);
		//terminate the expression and move to the next line
		rs->buf[--k] = 0;
		break;
	    } else if (lb_get(rs->b, rs->pos) == '*' && rs->pos.off > 0 && lb_get(rs->b, lbi_sub(rs->pos, 1)) == '/') {
		if (k == 1)
		    started = 0;
		rs->buf[--k] = 0;//set the slash to be a null terminator
		while (lb_inc(rs->b, &rs->pos)) {
		    if (lb_get(rs->b, rs->pos) == '*' && lb_get(rs->b, lbi_add(rs->pos, 1)) == '/') {
			rs->pos = lbi_add(rs->pos,2);
			break;
		    }
		}
	    }
	    //handle assignments
	    if (lb_get(rs->b, rs->pos) == '=') {
		rs->buf[k++] = 0;
		lval = trim_whitespace(rs->buf, NULL);
		init.off = k;
		//buf_off = k;
		rval_ind = k;
		continue;//don't copy the value into rs->buf
	    }
	    //if we encounter a block, then we need to make sure we include all of its contents, even if that block ends on another line
	    char match_tok = 0;
	    switch(lb_get(rs->b, rs->pos)) {
		case '(': match_tok = ')';break;
		case '[': match_tok = ']';break;
		case '{': match_tok = '}';break;
		case '\"': match_tok = '\"';break;
		case '\'': match_tok = '\'';break;
		default: break;
	    }
	    if (match_tok) {
		line_buffer_ind end;
		line_buffer* enc = lb_get_enclosed(rs->b, rs->pos, &end, lb_get(rs->b, rs->pos), match_tok, 1, 1);
		size_t tmp_len;
		char* tmp = lb_flatten(enc, ' ', &tmp_len);
		destroy_line_buffer(enc);
		//if we're still on the same line, then we need to continue until we reach the end. Otherwise, save everything inclosed and terminate
		if (end.line == init.line && end.off < len) {
		    rs->buf = append_to_line(rs->buf, &k, &rs->buf_size, tmp, end.off - rs->pos.off);
		    rs->pos = end;
		    init = rs->pos;
		    free(tmp);
		    //continue;//don't copy the value into rs->buf
		} else {
		    rs->buf = append_to_line(rs->buf, &k, &rs->buf_size, tmp, tmp_len);
		    free(tmp);
		    rs->pos = end;
		    break;//we're done reading this line since we jumped across a line
		}
	    }
	    //handle everything else
	    rs->buf[k++] = lb_get(rs->b, rs->pos);
	}
    }
    //only set the rval if we haven't done so already
    if (started) {
	value tmp_val = parse_value(c, rs->buf+rval_ind);
	if (tmp_val.type == VAL_ERR)
	    return tmp_val;
	//only append assignments
	if (lval)
	    set_value(c, lval, tmp_val, 0);
	else
	    cleanup_val(&tmp_val);
    } else {
	rs->pos.line += 1;
	rs->pos.off = 0;
    }
    return make_val_undef();
}

value read_from_lines(struct context* c, const line_buffer* b) {
    value er = make_val_undef();

    read_state rs = make_read_state(b);
    char* line = NULL;
    //iterate over each line in the file
    while (1) {
	if(line) free(line);
	line = lb_get_line(b, rs.pos);
	//check for class and function declarations
	char* dectype_start = token_block(line, "def");
	if (dectype_start) {
	    char* endptr;
	    cgs_func cur_func = parse_func(c, dectype_start + KEY_DEF_LEN, -1, &er, &endptr, 1);
	    //jump ahead until after the end of the function
	    if (er.type != VAL_ERR) {
		size_t i = 0;
		for (; endptr[i] && (endptr[i] == ' ' || endptr[i] == '\t' || endptr[i] == '\n'); ++i) (void)0;
		//now we actually create the function
		rs.pos.off = 0;
		line_buffer_ind end;
		line_buffer* fun_buf = lb_get_enclosed(b, rs.pos, &end, '{', '}', 0, 0);
		user_func* tmp = make_user_func_lb(cur_func, fun_buf);
		free(fun_buf);
		if (end.line == b->n_lines) {
		    er = make_val_error(E_BAD_SYNTAX, /*{*/"expected }");
		    goto exit;
		}
		value v;
		v.type = VAL_FUNC;
		v.val.f = tmp;
		v.n_els = b->n_lines - rs.pos.line;
		set_value(c, cur_func.name, v, 0);
		//we have to advance to the line after end of the function declaration
	    }
	} else {
	    er = read_single_line(c, &rs);
	    if (er.type == VAL_ERR) {
		if (c->parent == NULL) fprintf(stderr, "Error on line %lu: %s\n", rs.pos.line, er.val.e->msg);
		goto exit;
	    }
	}
	//if we're at the end of a line, try incrementing. If that doesn't work, then we've reached the end of the file.
	if (rs.pos.off >= lb_get_line_size(rs.b, rs.pos.line)) {
	    if (!lb_inc(b, &rs.pos))
		break;
	}
    }
exit:
    free(line);
    destroy_read_state(rs);
    return er;
}

/** ============================ user_func ============================ **/

user_func* make_user_func_lb(cgs_func sig, line_buffer* b) {
    user_func* uf = (user_func*)malloc(sizeof(user_func));
    uf->code_lines = copy_line_buffer(b);
    uf->call_sig = copy_func(sig);
    uf->exec = NULL;
    return uf;
}
user_func* make_user_func_ex(value (*p_exec)(context*, cgs_func)) {
    user_func* uf = (user_func*)malloc(sizeof(user_func));
    uf->code_lines = NULL;
    uf->call_sig.name = NULL;
    uf->call_sig.n_args = 0;
    uf->exec = p_exec;
    return uf;
}
user_func* copy_user_func(const user_func* o) {
    user_func* uf = (user_func*)malloc(sizeof(user_func));
    uf->code_lines = (o->code_lines)? copy_line_buffer(o->code_lines): NULL;
    uf->call_sig.name = (o->call_sig.name)? strdup(o->call_sig.name): NULL;
    uf->call_sig.n_args = o->call_sig.n_args;
    uf->exec = o->exec;
    return uf;
}
//deallocation
void destroy_user_func(user_func* uf) {
    cleanup_func(&(uf->call_sig));
    if (uf->code_lines)
	destroy_line_buffer(uf->code_lines);
    free(uf);
}
/*const char* token_names[] = {"if", "for", "while", "return"};
typedef enum {TOK_NONE, TOK_IF, TOK_FOR, TOK_WHILE, TOK_RETURN, N_TOKS} token_type;*/
value uf_eval(user_func* uf, context* c, cgs_func call) {
    if (uf->exec) {
	return (*uf->exec)(c, call);
    } else if (uf->call_sig.name && uf->call_sig.n_args > 0) {
	if (call.n_args != uf->call_sig.n_args)
	    return make_val_error(E_LACK_TOKENS, "%s() expected %lu arguments, got %lu", uf->call_sig.name, call.n_args, uf->call_sig.name);
	//setup a new scope with function arguments defined
	context* func_scope = make_context(c);
	for (size_t i = 0; i < uf->call_sig.n_args; ++i) {
	    set_value(func_scope, uf->call_sig.args[i].name, call.args[i].val, 0);
	}
	destroy_context(func_scope);
    }
    return make_val_error(E_BAD_VALUE, "function not implemented");
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