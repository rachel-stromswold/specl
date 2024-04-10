#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

extern "C" {
#include "speclang.h"
#define TEST_FNAME	"/tmp/speclang_test.spcl"
}
#define STRIFY(S) #S

bool spcl_true(spcl_val v) {
    if (v.type != VAL_NUM || v.val.x == 0)
	return false;
    return true;
}

void test_num(spcl_val v, double x) {
    CHECK(v.type == VAL_NUM);
    CHECK(v.val.x == x);
    CHECK(v.n_els == 1);
}

/**
 * save the mean and variance of values in the flattened array x[n] to mean and var. Takes only points where the index i satisfies off <= i % (dim+space) < off+dim these samples are treated as a vector. For instance mean_var(x, 6, 2, 1, 0, &mean, &var) will take the samples i=(0,1),(3,4). mean_var(x, 6, 2, 0, 0, &mean, &var) will take the samples i=(0,1),(2,3),(4,5)
 * x: the list of points
 * dim: the dimension of each sample
 * space: the separation between each sample
 * mean: a pointer where the mean is stored
 * var: a pointer where the variance is stored
 */
void mean_var(const double* x, size_t n, double* mean, double* var, size_t dim=1, size_t space=0, size_t off=0) {
    if (n == 0) {
	*mean = 0;
	*var = 0;
	return;
    }
    *mean = 0;
    size_t width = dim+space;
    for (size_t i = 0; i < n; ++i) {
	double tmp = 0;
	for (size_t j = 0; j < dim; ++j)
	    tmp += x[i*width+j+off]*x[i*width+j+off];
	*mean += sqrt(tmp);
    }
    *mean = *mean/(double)n;
    *var = 0;
    //avoid division by zero
    if (n == 1)
	return;
    for (size_t i = 0; i < n; ++i) {
	double tmp = 0;
	for (size_t j = 0; j < dim; ++j)
	    tmp += x[i*width+j+off]*x[i*width+j+off];
	tmp = sqrt(tmp);
	*var += (tmp - *mean)*(tmp - *mean);
    }
    *var = *var/(double)(n-1);
}

TEST_CASE("namecmp") {
    CHECK(namecmp("foo", "foo", 3) == 0);
    CHECK(namecmp("foo", "foot", 2) == 0);
    CHECK(namecmp("foo", "foot", 3) != 0);
    CHECK(namecmp("foo", "bar", 3) != 0);

    CHECK(namecmp("foo ", "foo", 3) == 0);
    CHECK(namecmp("foo ", "foo", SIZE_MAX) == 0);
    CHECK(namecmp("foo ", "foo+", 3) == 0);
    CHECK(namecmp("\tfoo ", "foo", 3) == 0);
    CHECK(namecmp("foo ", "-foo+", 3) == 0);
    CHECK(namecmp("ta ", "tan", 2) != 0);
}

TEST_CASE("spcl_val parsing") {
    char buf[SPCL_STR_BSIZE];
    spcl_inst* sc = make_spcl_inst(NULL);
    spcl_val tmp_val;

    SUBCASE("Reading numbers to spcl_vals works") {
	//test integers
	strncpy(buf, "1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "12", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 12);
	cleanup_spcl_val(&tmp_val);
	//test floats
	strncpy(buf, ".25", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, .25);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "+1.25", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.25);
	cleanup_spcl_val(&tmp_val);
	//test scientific notation
	strncpy(buf, ".25e10", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 0.25e10);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "1.25e+10", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.25e10);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "-1.25e-10", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, -1.25e-10);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("Reading strings to spcl_vals works") {
	//test a simple string
	strncpy(buf, "\"foo\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo") == 0);
	cleanup_spcl_val(&tmp_val);
	//test a string with whitespace
	strncpy(buf, "\" foo bar \"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, " foo bar ") == 0);
	cleanup_spcl_val(&tmp_val);
	//test a string with stuff inside it
	strncpy(buf, "\"foo(bar)\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo(bar)") == 0);
	cleanup_spcl_val(&tmp_val);
	//test a string with an escaped string
	strncpy(buf, "\"foo\\\"bar\\\" \"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo\\\"bar\\\" ") == 0);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("Reading lists to spcl_vals works") {
	//test one element lists
	strncpy(buf, "[\"foo\"]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.l[0].type == VAL_STR);
	CHECK(strcmp(tmp_val.val.l[0].val.s, "foo") == 0);
	cleanup_spcl_val(&tmp_val);
	//test two element lists
	strncpy(buf, "[\"foo\", 1]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 2);
	CHECK(tmp_val.val.l[0].type == VAL_STR);
	CHECK(strcmp(tmp_val.val.l[0].val.s, "foo") == 0);
	CHECK(tmp_val.val.l[1].type == VAL_NUM);
	CHECK(tmp_val.val.l[1].val.x == 1);
	cleanup_spcl_val(&tmp_val);
	//test lists of lists
	strncpy(buf, "[[1,2,3], [\"4\", \"5\", \"6\"]]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 2);
	{
	    //check the first sublist
	    spcl_val element = tmp_val.val.l[0];
	    CHECK(element.type == VAL_LIST);
	    CHECK(element.n_els == 3);
	    CHECK(element.val.l != NULL);
	    CHECK(element.val.l[0].type == VAL_NUM);
	    CHECK(element.val.l[0].val.x == 1);
	    CHECK(element.val.l[1].type == VAL_NUM);
	    CHECK(element.val.l[1].val.x == 2);
	    CHECK(element.val.l[2].type == VAL_NUM);
	    CHECK(element.val.l[2].val.x == 3);
	    //check the second sublist
	    element = tmp_val.val.l[1];
	    CHECK(element.type == VAL_LIST);
	    CHECK(element.n_els == 3);
	    CHECK(element.val.l != NULL);
	    CHECK(element.val.l[0].type == VAL_STR);
	    CHECK(strcmp(element.val.l[0].val.s, "4") == 0);
	    CHECK(element.val.l[1].type == VAL_STR);
	    CHECK(strcmp(element.val.l[1].val.s, "5") == 0);
	    CHECK(element.val.l[2].type == VAL_STR);
	    CHECK(strcmp(element.val.l[2].val.s, "6") == 0);
	}
	cleanup_spcl_val(&tmp_val);
	//test lists interpretations
	strncpy(buf, "[[i*2 for i in range(2)], [i*2-1 for i in range(1,3)], [x for x in range(1,3,0.5)]]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 3);
	{
	    //check the first sublist
	    spcl_val element = tmp_val.val.l[0];
	    REQUIRE(element.type == VAL_LIST);
	    REQUIRE(element.n_els == 2);
	    REQUIRE(element.val.l != NULL);
	    CHECK(element.val.l[0].type == VAL_NUM);
	    CHECK(element.val.l[0].val.x == 0);
	    CHECK(element.val.l[1].type == VAL_NUM);
	    CHECK(element.val.l[1].val.x == 2);
	    //check the second sublist
	    element = tmp_val.val.l[1];
	    REQUIRE(element.type == VAL_LIST);
	    REQUIRE(element.n_els == 2);
	    REQUIRE(element.val.l != NULL);
	    CHECK(element.val.l[0].type == VAL_NUM);
	    CHECK(element.val.l[0].val.x == 1);
	    CHECK(element.val.l[1].type == VAL_NUM);
	    CHECK(element.val.l[1].val.x == 3);
	    //check the third sublist
	    element = tmp_val.val.l[2];
	    REQUIRE(element.type == VAL_LIST);
	    REQUIRE(element.n_els == 4);
	    REQUIRE(element.val.l != NULL);
	    CHECK(element.val.l[0].type == VAL_NUM);
	    CHECK(element.val.l[0].val.x == 1);
	    CHECK(element.val.l[1].type == VAL_NUM);
	    CHECK(element.val.l[1].val.x == 1.5);
	    CHECK(element.val.l[2].type == VAL_NUM);
	    CHECK(element.val.l[2].val.x == 2);
	    CHECK(element.val.l[3].type == VAL_NUM);
	    CHECK(element.val.l[3].val.x == 2.5);
	}
	cleanup_spcl_val(&tmp_val);
	//test nested list interpretations
	strncpy(buf, "[[x*y for x in range(1,6)] for y in range(5)]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_LIST);
	REQUIRE(tmp_val.val.l != NULL);
	REQUIRE(tmp_val.n_els == 5);
	for (size_t yy = 0; yy < tmp_val.n_els; ++yy) {
	    CHECK(tmp_val.val.l[yy].type == VAL_LIST);
	    CHECK(tmp_val.val.l[yy].n_els == 5);
	    for (size_t xx = 0; xx < tmp_val.val.l[yy].n_els; ++xx) {
		CHECK(tmp_val.val.l[yy].val.l[xx].type == VAL_NUM);
		CHECK(tmp_val.val.l[yy].val.l[xx].val.x == (xx+1)*yy);
	    }
	}
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("Reading vectors to spcl_vals works") {
	//test one element lists
	strncpy(buf, "vec(1.2, 3.4,56.7)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.val.a != NULL);
	CHECK(tmp_val.val.a[0] == doctest::Approx(1.2));
	CHECK(tmp_val.val.a[1] == doctest::Approx(3.4));
	CHECK(tmp_val.val.a[2] == doctest::Approx(56.7));
	cleanup_spcl_val(&tmp_val);

	strncpy(buf, "array([1.2, 3.4,56.7])", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.val.a != NULL);
	CHECK(tmp_val.val.a[0] == doctest::Approx(1.2));
	CHECK(tmp_val.val.a[1] == doctest::Approx(3.4));
	CHECK(tmp_val.val.a[2] == doctest::Approx(56.7));
	cleanup_spcl_val(&tmp_val);

	strncpy(buf, "array([[0, 1, 2], [3, 4, 5], [6, 7, 8]])", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_MAT);
	REQUIRE(tmp_val.val.l != NULL);
	REQUIRE(tmp_val.n_els == 3);
	for (size_t i = 0; i < 3; ++i) {
	    REQUIRE(tmp_val.val.l[i].type == VAL_ARRAY);
	    REQUIRE(tmp_val.val.l[i].n_els == 3);
	    for (size_t j = 0; j < 3; ++j) {
		CHECK(tmp_val.val.l[i].val.a[j] == i*3 + j);
	    }
	}
	cleanup_spcl_val(&tmp_val);
    }
    destroy_spcl_inst(sc);
}

TEST_CASE("string handling") {
    const size_t STR_SIZE = 64;
    spcl_inst* sc = make_spcl_inst(NULL);
    char buf[STR_SIZE];memset(buf, 0, STR_SIZE);
    //check that parsing an empty (or all whitespace) string gives VAL_UNDEF
    spcl_val tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    cleanup_spcl_val(&tmp_val);
    strncpy(buf, " ", STR_SIZE);
    tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    cleanup_spcl_val(&tmp_val);
    strncpy(buf, "\t  \t\n", STR_SIZE);
    tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    cleanup_spcl_val(&tmp_val);
    //now check that strings are the right length
    for (size_t i = 1; i < STR_SIZE-1; ++i) {
	for (size_t j = 0; j < i; ++j)
	    buf[j] = 'a';
	buf[0] = '\"';
	buf[i] = '\"';
	buf[i+1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.n_els == i);
	cleanup_spcl_val(&tmp_val);
	CHECK(tmp_val.n_els == 0);
    }
    destroy_spcl_inst(sc);
}

TEST_CASE("operations") {
    spcl_inst* sc = make_spcl_inst(NULL);
    char buf[SPCL_STR_BSIZE];
    SUBCASE("Arithmetic works") {
        //single operations
        strncpy(buf, "1+1.1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        spcl_val tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.1);
        strncpy(buf, "2-1.25", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 0.75);
        strncpy(buf, "2*1.1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.2);
        strncpy(buf, "2.2/2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.1);
        //order of operations
        strncpy(buf, "2*2-1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 3);
        strncpy(buf, "1+3/2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.5);
        strncpy(buf, "(1+3)/2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.0);
        strncpy(buf, "-(1+3)/2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, -2.0);
        strncpy(buf, "2*9/4*3", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.5);
	strncpy(buf, "-2*9^2/4*3", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, -13.5);
	strncpy(buf, "(-2*9)^2/4*3", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 27);
    }
    SUBCASE("Comparisons work") {
	//create a single true and false, this makes things easier
        strncpy(buf, "2 == 2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "1 == 2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "[2, 3] == [2,3]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "[2, 3, 4] == [2, 3]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "[2, 3, 4] == [2, 3, 5]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "\"apple\" == \"apple\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "\"apple\" == \"banana\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_NUM);
        CHECK(!spcl_true(tmp_val));
    }
    SUBCASE("String concatenation works") {
        //single operations
        strncpy(buf, "\"foo\"+\"bar\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        spcl_val tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_STR);
        CHECK(strcmp(tmp_val.val.s, "foobar") == 0);
	CHECK(tmp_val.n_els == 7);
        cleanup_spcl_val(&tmp_val);
	CHECK(tmp_val.n_els == 0);
    }
    SUBCASE("comparisons work") {
	//equality
	strncpy(buf, "1 == 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "1 == 2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	//geq
	strncpy(buf, "1 >= 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(spcl_true(tmp));
	//greater than
	strncpy(buf, "1 > 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "-1 > 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	//leq
	strncpy(buf, "1 <= 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//less than
	strncpy(buf, "1 < 2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "1 < 1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "1 < -1", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
    }
    SUBCASE("boolean operations work") {
	//or
	strncpy(buf, "false || false", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true || false", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "false || true", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "true || true", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//and
	strncpy(buf, "false && false", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true && false", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "false && true", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true && true", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//not
	strncpy(buf, "!false", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "!true", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
    }
    SUBCASE("Ternary operators work") {
	strncpy(buf, "(false) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(true) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	strncpy(buf, "(1 == 2) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(2 == 2) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	strncpy(buf, "(1 > 2) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(1 < 2) ? 100 : 200", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	//check with pairs of strings
	strncpy(buf, "(1 < 2) ? \"100\" : \"200\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "100") == 0);
	strncpy(buf, "(1 > 2) ? \"100\" : \"200\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "200") == 0);
	//test graceful failure conditions
	strncpy(buf, "(1 < 2) ? 100", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "expected \':\' in ternary") == 0);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("Missing end graceful failure") {
	strncpy(buf, "[1,2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "expected ]") == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "a(1,2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "expected )") == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "\"1,2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "expected \"") == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "1,2]", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "unexpected ]") == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "1,2)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "unexpected )") == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "1,2\"", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	WARN(strcmp(tmp_val.val.e->msg, "expected \"") == 0);
	cleanup_spcl_val(&tmp_val);
    }
    destroy_spcl_inst(sc);
}

TEST_CASE("builtin functions") {
    char buf[SPCL_STR_BSIZE];
    spcl_val tmp_val;
    spcl_inst* sc = make_spcl_inst(NULL);

    SUBCASE("range()") {
	strncpy(buf, "range(4)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 4);
	for (size_t i = 0; i < 4; ++i)
	    CHECK(tmp_val.val.a[i] == i);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "range(1,4)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 3);
	for (size_t i = 0; i < 3; ++i)
	    CHECK(tmp_val.val.a[i] == i+1);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "range(1,4,0.5)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 6);
	for (size_t i = 0; i < 6; ++i)
	    CHECK(tmp_val.val.a[i] == 0.5*i+1);
	cleanup_spcl_val(&tmp_val);
        //graceful failure cases
        strncpy(buf, "range()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	//check that divisions by zero are avoided
        strncpy(buf, "range(0,1,0)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_VALUE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "range(\"1\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "range(0.5,\"1\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "range(0.5,1,\"2\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("linspace()") {
        strncpy(buf, "linspace(1,2,5)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_ARRAY);
        CHECK(tmp_val.n_els == 5);
        for (size_t i = 0; i < 4; ++i) {
            CHECK(tmp_val.val.a[i] == 1.0+0.25*i);
        }
        cleanup_spcl_val(&tmp_val);
        strncpy(buf, "linspace(2,1,5)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_ARRAY);
        CHECK(tmp_val.n_els == 5);
        for (size_t i = 0; i < 4; ++i) {
            CHECK(tmp_val.val.a[i] == 2.0-0.25*i);
        }
        cleanup_spcl_val(&tmp_val);
        //graceful failure cases
        strncpy(buf, "linspace(2,1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "linspace(2,1,1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_VALUE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "linspace(\"2\",1,1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "linspace(2,\"1\",1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
        strncpy(buf, "linspace(2,1,\"1\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("flatten()") {
	strncpy(buf, "flatten([])", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 0);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "flatten([1,2,3])", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 3);
	for (size_t i = 0; i < 3; ++i) {
	    test_num(tmp_val.val.l[i], i+1);
	}
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "flatten([[1,2,3],[4,5],6])", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 6);
	for (size_t i = 0; i < 6; ++i)
	    test_num(tmp_val.val.l[i], i+1);

	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("math functions") {
	strncpy(buf, "math.sin(3.1415926535/2)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.sin(3.1415926535/6)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(0.5));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.cos(3.1415926535/2)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(0.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.cos(3.1415926535/6)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.sqrt(3)/2", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	spcl_val sqrt_val = spcl_parse_line(sc, buf);
	CHECK(sqrt_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(sqrt_val.val.x));
	strncpy(buf, "math.tan(3.141592653589793/4)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	strncpy(buf, "math.tan(0)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(0.0));
	strncpy(buf, "math.exp(0)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.exp(1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(2.718281828));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.log(1)", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == 0);
	CHECK(tmp_val.n_els == 1);
	//failure conditions
	strncpy(buf, "math.sin()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.cos()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.tan()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.exp()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.sqrt()", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_LACK_TOKENS);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.sin(\"a\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.cos(\"a\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.tan(\"a\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.exp(\"a\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
	strncpy(buf, "math.sqrt(\"a\")", SPCL_STR_BSIZE);buf[SPCL_STR_BSIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	WARN(tmp_val.val.e->c == E_BAD_TYPE);
	cleanup_spcl_val(&tmp_val);
    }
    SUBCASE("assertions") {
	strncpy(buf, "assert(1)", SPCL_STR_BSIZE);
	spcl_val tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	CHECK(tmp.n_els == 1);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(true)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(false)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(1 <= 3)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x != 0);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(1 == 1)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x != 0);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(1 > 3, \"1 is not greater than 3\")", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	WARN(strcmp(tmp.val.e->msg, "1 is not greater than 3") == 0);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "isdef(apple)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 0);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "isdef(math.pi)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(apple)", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	cleanup_spcl_val(&tmp);
	strncpy(buf, "assert(isdef(apple))", SPCL_STR_BSIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	cleanup_spcl_val(&tmp);
    }
    destroy_spcl_inst(sc);
}

#if SPCL_DEBUG_LVL>0
char* fetch_fs_line(const spcl_fstream* fs, size_t line, size_t off) {
    lbi s = make_lbi(line,off);
    lbi e = make_lbi(line, fs->line_sizes[line]);
    return fs_get_line(fs, s, e, NULL);
}
#endif

void write_test_file(const char** lines, size_t n_lines, const char* fname) {
    FILE* f = fopen(fname, "w");
    for (size_t i = 0; i < n_lines; ++i)
	fprintf(f, "%s\n", lines[i]);
    fclose(f);
}

//this function has a bunch of stuff that's only marked visible in debug builds, so we just ommit it for release
#if SPCL_DEBUG_LVL>0
TEST_CASE("spcl_fstream fs_get_enclosed") {
    const char* fun_contents[] = {"{", "if a > 5 {", "return 1", "}", "return 0", ""};
    const char* if_contents[] = {"{", "return 1", ""};
    char* strval = NULL;

    SUBCASE("open brace on a different line") {
	const char* lines[] = { "fn test_fun(a)", "{", "if a > 5 {", "return 1", "}", "return 0", "}" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	//check the lines (curly brace on new line)
	spcl_fstream* fs = make_spcl_fstream(TEST_FNAME);
	for (size_t i = 0; i < n_lines; ++i) {
	    strval = fetch_fs_line(fs, i, 0);
	    CHECK(strcmp(lines[i], strval) == 0);
	    free(strval);
	}
	//find the block that says fn
	read_state rs = make_read_state(fs, make_lbi(0,0), make_lbi(1,0));
	spcl_key tkey = get_keyword(&rs);
	CHECK(tkey == KEY_FN);
	CHECK(rs.start.line == 0);
	CHECK(rs.start.off == 2);
	//find the parenthesis
	lbi op_loc, open_ind, close_ind, new_end;
	spcl_val er = find_operator(rs, &op_loc, &open_ind, &close_ind, &new_end);
	CHECK(er.type != VAL_ERR);
	CHECK(open_ind.line == 0);CHECK(open_ind.off == strlen("fn test_fun"));
	CHECK(close_ind.line == 0);CHECK(close_ind.off == fs->line_sizes[0]-1);
	CHECK(lbicmp(op_loc, new_end) >= 0);
	//test the braces around the function
	er = find_operator(make_read_state(fs, make_lbi(1,0), make_lbi(6,1)), &op_loc, &open_ind, &close_ind, &new_end);
	CHECK(er.type != VAL_ERR);
	CHECK(open_ind.line == 1);CHECK(open_ind.off == 0);
	CHECK(close_ind.line == 6);CHECK(close_ind.off == 0);
	spcl_fstream* b_fun_con = fs_get_enclosed(fs, open_ind, close_ind);
	for (size_t i = 0; i < b_fun_con->n_lines; ++i) {
	    strval = fetch_fs_line(b_fun_con, i,0);
	    CHECK(strcmp(fun_contents[i], strval) == 0);
	    free(strval);
	}
	//check the braces around the if statement
	er = find_operator(make_read_state(fs, make_lbi(2,0), close_ind), &op_loc, &open_ind, &close_ind, &new_end);
	CHECK(er.type != VAL_ERR);
	CHECK(open_ind.line == 2);CHECK(open_ind.off == strlen("if a > 5 "));
	CHECK(close_ind.line == 4);CHECK(close_ind.off == 0);
	spcl_fstream* b_if_con = fs_get_enclosed(fs, open_ind, close_ind);
	for (size_t i = 0; i < b_if_con->n_lines; ++i) {
	    strval = fetch_fs_line(b_if_con, i,0);
	    CHECK(strcmp(if_contents[i], strval) == 0);
	    free(strval);
	}
	destroy_spcl_fstream(b_if_con);
	destroy_spcl_fstream(b_fun_con);
	destroy_spcl_fstream(fs);
    }
}
#endif

spcl_val test_fun_call(spcl_inst* c, spcl_fn_call f) {
    spcl_val ret;
    if (f.n_args < 1)
	return spcl_make_err(E_LACK_TOKENS, "expected 1 argument");
    if (f.args[0].type != VAL_NUM)
	return spcl_make_err(E_LACK_TOKENS, "only works with numbers");
    double a = f.args[0].val.x;
    if (a > 5) {
	ret.type = VAL_INST;
	ret.val.c = make_spcl_inst(c);
	spcl_set_val(ret.val.c, "name", spcl_make_str("hi", 3), 0);
	return ret;
    }
    return f.args[0];
}

spcl_val test_fun_gamma(spcl_inst* c, spcl_fn_call f) {
    (void)c;
    if (f.n_args < 1)
	return spcl_make_err(E_LACK_TOKENS, "expected 1 argument");
    if (f.args[0].type != VAL_NUM)
	return spcl_make_err(E_LACK_TOKENS, "only works with numbers");
    double a = f.args[0].val.x;
    return spcl_make_num(sqrt(1 - a*a));
}

TEST_CASE("spcl_inst lookups") {
    const char* letters = "etaoin";
    size_t n_letters = strlen(letters);
    const size_t GEN_LEN = 4;
    char name[GEN_LEN+1];
    memset(name, 0, GEN_LEN+1);
    spcl_inst* c = make_spcl_inst(NULL);
    spcl_set_val(c, "tao", spcl_make_str("tao", 4), 0);
    size_t n_combs = 1;
    for (size_t i = 0; i < GEN_LEN; ++i)
	n_combs *= n_letters;

    //add a whole bunch of words using the most common letters
    spcl_val v;
    size_t before_size = c->n_memb;
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < n_combs; ++i) {
	size_t j = i;
	size_t k = 0;
	do {
	    name[k++] = letters[j % n_letters];
	    j /= n_letters;
	} while (j && k < GEN_LEN);
	spcl_set_val(c, name, spcl_make_num(i), 1);
	v = spcl_find(c, name);
	test_num(v, i);
    }
    auto end = std::chrono::steady_clock::now();
    double time = std::chrono::duration <double, std::milli> (end-start).count();
    printf("took %f ms to set %lu elements\n", time, n_combs);
    //lookup something not in the spcl_inst, check that we only added n_combs-1 elements because we added one match explicitly before
    CHECK(c->n_memb == n_combs+before_size-1);
    v = spcl_find(c, "vetaon");
    CHECK(v.type == VAL_UNDEF);
    CHECK(v.val.x == 0);
    CHECK(v.n_els == 0);
    destroy_spcl_inst(c);
}

TEST_CASE("spcl_inst parsing") {
    SUBCASE ("without nesting") {
	const char* lines[] = { "a = 1", "\"b\"", " c = [\"d\", \"e\"]" }; 
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	spcl_val er;
	spcl_inst* c = spcl_inst_from_file(TEST_FNAME, &er, 0, NULL);
	CHECK(er.type != VAL_ERR);
	//lookup the named spcl_vals
	spcl_val val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_NUM);
	CHECK(val_a.val.x == 1);
	spcl_val val_c = spcl_find(c, "c");
	CHECK(val_c.type == VAL_LIST);
	CHECK(val_c.n_els == 2);
	CHECK(val_c.val.l[0].type == VAL_STR);
	CHECK(val_c.val.l[1].type == VAL_STR);
	destroy_spcl_inst(c);
    }
    SUBCASE ("with nesting") {
	const char* lines[] = { "a = {name = \"apple\";", "spcl_vals = [20, 11]}", "b = a.spcl_vals[0]", "c = a.spcl_vals[1] + a.spcl_vals[0]+1" }; 
	size_t n_lines = sizeof(lines)/sizeof(char*);
	spcl_fstream* fs = make_spcl_fstream(NULL);
	for (size_t i = 0; i < n_lines; ++i)
	    spcl_fstream_append(fs, lines[i]);
	spcl_inst* c = make_spcl_inst(NULL);
	spcl_val er = spcl_read_lines(c, fs);
	CHECK(er.type == VAL_UNDEF);
	//lookup the named spcl_vals
	spcl_val val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_INST);
	spcl_val val_a_name = spcl_find(val_a.val.c, "name");
	CHECK(val_a_name.type == VAL_STR);
	CHECK(strcmp(val_a_name.val.s, "apple") == 0);
	spcl_val val_a_spcl_val = spcl_find(val_a.val.c, "spcl_vals");
	REQUIRE(val_a_spcl_val.type == VAL_LIST);
	REQUIRE(val_a_spcl_val.n_els == 2);
	CHECK(val_a_spcl_val.val.l[0].type == VAL_NUM);
	CHECK(val_a_spcl_val.val.l[1].type == VAL_NUM);
	CHECK(val_a_spcl_val.val.l[0].val.x == 20);
	CHECK(val_a_spcl_val.val.l[1].val.x == 11);
	spcl_val val_b = spcl_find(c, "b");
	CHECK(val_b.type == VAL_NUM);
	CHECK(val_b.val.x == 20);
	spcl_val val_c = spcl_find(c, "c");
	CHECK(val_c.type == VAL_NUM);
	CHECK(val_c.val.x == 32);
	destroy_spcl_fstream(fs);
	destroy_spcl_inst(c);
    }
    SUBCASE ("external user defined functions") {
	const char* fun_name = "test_fun";
	char* tmp_name = strdup(fun_name);

	const char* lines[] = { "a = test_fun(1);b=test_fun(10)" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	spcl_fstream* b_1 = make_spcl_fstream(TEST_FNAME);
	spcl_inst* c = make_spcl_inst(NULL);
	spcl_add_fn(c, test_fun_call, "test_fun");
	spcl_val er = spcl_read_lines(c, b_1);
	REQUIRE(er.type != VAL_ERR);
	//make sure that the function is there
	spcl_val val_fun = spcl_find(c, "test_fun");
	CHECK(val_fun.type == VAL_FN);
	//make sure that the number spcl_val a is there
	spcl_val val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_NUM);
	CHECK(val_a.val.x == 1);
	spcl_val val_b = spcl_find(c, "b");
	CHECK(val_b.type == VAL_INST);
	spcl_val val_b_name = spcl_find(val_b.val.c, "name");
	CHECK(val_b_name.type == VAL_STR);
	CHECK(strcmp(val_b_name.val.s, "hi") == 0);
	free(tmp_name);
	destroy_spcl_inst(c);
	destroy_spcl_fstream(b_1);
    }
    SUBCASE ("internal user defined functions") {
	const char* fun_name = "test_fun";
	char* tmp_name = strdup(fun_name);

	const char* lines[] = {
	    "test_fun = fn(i) {",
	    "return (i < 2)? \"a\" : 2",
	    "}",
	    "a = test_fun(1);b=test_fun(10)" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	//read the file and check for errors
	spcl_val er;
	spcl_inst* c = spcl_inst_from_file(TEST_FNAME, &er, 0, NULL);
	REQUIRE(er.type != VAL_ERR);
	//make sure that the function is there
	spcl_val val_fun = spcl_find(c, "test_fun");
	CHECK(val_fun.type == VAL_FN);
	//make sure that the number spcl_val a is there
	spcl_val val_a = spcl_find(c, "a");
	REQUIRE(val_a.type == VAL_STR);
	CHECK(strcmp(val_a.val.s, "a") == 0);
	spcl_val val_b = spcl_find(c, "b");
	test_num(val_b, 2);
	destroy_spcl_inst(c);
    }
    SUBCASE ("stress test") {
	//first we add a bunch of arbitrary variables to make searching harder for the parser
	const char* lines1[] = {
	    "Vodkis=1","Pagne=2","Meadaj=3","whis=4","nac4=5","RaKi=6","gyn=7","cid=8","Daiqui=9","Mooshi=10","Magnac=2","manChe=3","tes=4","Bourbu=5","magna=6","sak=7","Para=8","Keffi=9","Guino=10","Uuqax=11","Thraxeods=12","Trinzoins=13","gheds=14","theSoild=15","vengirs=16",
	    "y = 2.0", "xs = linspace(0, y, 10000)", "arr1 = [math.sin(6*x/y) for x in xs]" };
	size_t n_lines1 = sizeof(lines1)/sizeof(char*);
	write_test_file(lines1, n_lines1, TEST_FNAME);
	spcl_fstream* b_1 = make_spcl_fstream(TEST_FNAME);
	const char* lines2[] = { "arr2 = [gam(x/y) for x in xs]" };
	size_t n_lines2 = sizeof(lines2)/sizeof(char*);
	write_test_file(lines2, n_lines2, TEST_FNAME);
	spcl_fstream* b_2 = make_spcl_fstream(TEST_FNAME);
	spcl_inst* c = make_spcl_inst(NULL);
	spcl_val er = spcl_read_lines(c, b_1);
	CHECK(er.type != VAL_ERR);
	spcl_val tmp_f = spcl_make_fn("gam", 1, &test_fun_gamma);
	spcl_set_val(c, "gam", tmp_f, 1);
	cleanup_spcl_val(&tmp_f);
	er = spcl_read_lines(c, b_2);
	CHECK(er.type != VAL_ERR);
	destroy_spcl_inst(c);
	destroy_spcl_fstream(b_1);
	destroy_spcl_fstream(b_2);
    }
}
static const valtype SRC_SIG[] = {VAL_STR, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_INST};
spcl_val spcl_gen_gaussian_source(spcl_inst* c, spcl_fn_call f) {
    spcl_sigcheck_opts(f, 6, SRC_SIG);
    /*spcl_val ret = check_signature(f, SIGLEN(SRC_SIG), SIGLEN(SRC_SIG)+3, SRC_SIG);
    if (ret.type)
	return ret;
    if (f.args[f.n_args-1].val.type != VAL_INST)
	return spcl_make_err(E_BAD_TYPE, "");*/
    spcl_val ret = spcl_make_inst(c, "Gaussian_source");
    spcl_set_val(ret.val.c, "component", f.args[0], 1);
    spcl_set_val(ret.val.c, "wavelength", f.args[1], 0);
    spcl_set_val(ret.val.c, "amplitude", f.args[2], 0);
    spcl_set_val(ret.val.c, "width", f.args[3], 0);
    spcl_set_val(ret.val.c, "phase", f.args[4], 0);
    //read additional parameters
    spcl_set_val(ret.val.c, "cutoff", (f.n_args>6)? f.args[5]: spcl_make_num(5), 0);
    spcl_set_val(ret.val.c, "start_time", (f.n_args>7)? f.args[6]: spcl_make_num(5), 0);
    spcl_set_val(ret.val.c, "region", f.args[f.n_args-1], 1);
    return ret;
}
static const valtype BOX_SIG[] = {VAL_ARRAY, VAL_ARRAY};
spcl_val spcl_gen_box(spcl_inst* c, spcl_fn_call f) {
    spcl_sigcheck(f, BOX_SIG);
    /*spcl_val ret = check_signature(f, SIGLEN(BOX_SIG), SIGLEN(BOX_SIG)+3, BOX_SIG);
    if (ret.type)
	return ret;*/
    spcl_val ret = spcl_make_inst(c, "Box");
    spcl_set_val(ret.val.c, "pt_1", f.args[0], 1);
    spcl_set_val(ret.val.c, "pt_2", f.args[1], 1);
    return ret;
}
static const valtype QUAD_SIG[] = {VAL_NUM};
spcl_val spcl_quad_trap(spcl_inst* c, spcl_fn_call f) {
    spcl_sigcheck(f, QUAD_SIG);
    spcl_val ret = spcl_make_inst(c, "quad_pot");
    spcl_set_val(ret.val.c, "k", f.args[0], 0);
    return ret;
}
void setup_geometry_inst(spcl_inst* con) {
    //we have to set up the spcl_inst with all of our functions
    spcl_add_fn(con, spcl_gen_gaussian_source, "Gaussian_source");
    spcl_add_fn(con, spcl_gen_box, "Box");
    spcl_add_fn(con, spcl_quad_trap, "quad_pot");
}

TEST_CASE("file parsing") {
    const size_t N_FLTS = 10;
    char buf[SPCL_STR_BSIZE];
    spcl_fstream* fs = make_spcl_fstream(TEST_GEOM_NAME);
    spcl_inst* c = make_spcl_inst(NULL);
    setup_geometry_inst(c);
    spcl_val er = spcl_read_lines(c, fs);
    CHECK(er.type != VAL_ERR);
    spcl_val v = spcl_find(c, "offset");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 0.2);
    v = spcl_find(c, "lst");
    CHECK(v.type == VAL_LIST);
    v = spcl_find(c, "sum_lst");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 11);
    v = spcl_find(c, "prod_lst");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 24.2);
    v = spcl_find(c, "acid_test");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 16);
    v = spcl_find(c, "acid_res");
    CHECK(spcl_true(v));
    //lookup only does a shallow copy so we don't need to free

    strncpy(buf, "gs.__type__ == \"Gaussian_source\"", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.component == \"Ey\"", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.wavelength == 1.5", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.amplitude == 7", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.width == 3", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.phase == 0.75", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.cutoff == 6", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.start_time == 5.2", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.__type__ == \"Box\"", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.pt_1 == vec(0,0,.2)", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.pt_2 == vec(.4, 0.4, .2)", SPCL_STR_BSIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    //now try using the builtin find functions
    int n;
    unsigned len;
    double flts[N_FLTS];
    spcl_inst* sub;
    REQUIRE(spcl_find_object(c, "gs", "Gaussian_source", &sub) == 0);
    //string lookups
    CHECK(spcl_find_c_str(sub, "component", buf, SPCL_STR_BSIZE) == 3);
    CHECK(strcmp(buf, "Ey") == 0);
    CHECK(spcl_find_float(sub, "wavelength", flts) == 0);
    CHECK(flts[0] == 1.5);
    CHECK(spcl_find_int(sub, "wavelength", &n) == 0);
    CHECK(n == 1);
    CHECK(spcl_find_uint(sub, "amplitude", &len) == 0);
    CHECK(len == 7);
    //array lookups
    REQUIRE(spcl_find_object(sub, "region", "Box", &sub) == 0);
    REQUIRE(spcl_find_c_darray(sub, "pt_1", flts, N_FLTS) == 3);
    CHECK(flts[0] == 0);CHECK(flts[1] == 0);CHECK(flts[2] == 0.2);
    REQUIRE(spcl_find_c_darray(sub, "pt_2", flts, N_FLTS) == 3);
    CHECK(flts[0] == 0.4);CHECK(flts[1] == 0.4);CHECK(flts[2] == 0.2);
    REQUIRE(spcl_find_object(c, "potential", "quad_pot", &sub) == 0);
    CHECK(spcl_find_float(sub, "k", flts) == 0);
    CHECK(flts[0] == 4);
    //cleanup
    destroy_spcl_fstream(fs);
    destroy_spcl_inst(c);
}

TEST_CASE("benchmarks") {
    const size_t N_RUNS = 100;
    double times[N_RUNS];
    double mean, var;
    spcl_val er;
    //run assertions
    for (size_t i = 0; i < N_RUNS; ++i) {
	auto start = std::chrono::steady_clock::now();
	spcl_inst* c = spcl_inst_from_file(TEST_ASSERT_NAME, &er, 0, NULL);
	auto end = std::chrono::steady_clock::now();
	times[i] = std::chrono::duration <double, std::milli> (end-start).count();
	if (i == 0)
	    CHECK(er.type == VAL_UNDEF);
	cleanup_spcl_val(&er);
	destroy_spcl_inst(c);
    }
    mean_var(times, N_RUNS, &mean, &var);
    printf("assertions.spcl evaluated in %f\xc2\xb1%f ms\n", mean, sqrt(var));
    //run benchmarks
    for (size_t i = 0; i < N_RUNS; ++i) {
	auto start = std::chrono::steady_clock::now();
	spcl_inst* c = spcl_inst_from_file(TEST_BENCH_NAME, &er, 0, NULL);
	auto end = std::chrono::steady_clock::now();
	times[i] = std::chrono::duration <double, std::milli> (end-start).count();
	cleanup_spcl_val(&er);
	destroy_spcl_inst(c);
    }
    mean_var(times, N_RUNS, &mean, &var);
    printf("benchmark.spcl evaluated in %f\xc2\xb1%f ms\n", mean, sqrt(var));
}
