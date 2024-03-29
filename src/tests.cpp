#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

extern "C" {
#include "read.h"
}

#define TEST_FNAME	"/tmp/speclang_test.spcl"

bool spcl_true(value v) {
    if (v.type != VAL_NUM || v.val.x == 0)
	return false;
    return true;
}

void test_num(value v, double x) {
    CHECK(v.type == VAL_NUM);
    CHECK(v.val.x == x);
    CHECK(v.n_els == 1);
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

TEST_CASE("value parsing") {
    char buf[BUF_SIZE];
    spcl_inst* sc = make_spcl_inst(NULL);
    value tmp_val;

    SUBCASE("Reading numbers to values works") {
	//test integers
	strncpy(buf, "1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "12", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 12);
	spcl_cleanup_val(&tmp_val);
	//test floats
	strncpy(buf, ".25", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, .25);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "+1.25", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.25);
	spcl_cleanup_val(&tmp_val);
	//test scientific notation
	strncpy(buf, ".25e10", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 0.25e10);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "1.25e+10", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.25e10);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "-1.25e-10", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, -1.25e-10);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("Reading strings to values works") {
	//test a simple string
	strncpy(buf, "\"foo\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo") == 0);
	spcl_cleanup_val(&tmp_val);
	//test a string with whitespace
	strncpy(buf, "\" foo bar \"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, " foo bar ") == 0);
	spcl_cleanup_val(&tmp_val);
	//test a string with stuff inside it
	strncpy(buf, "\"foo(bar)\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo(bar)") == 0);
	spcl_cleanup_val(&tmp_val);
	//test a string with an escaped string
	strncpy(buf, "\"foo\\\"bar\\\" \"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_STR);
	CHECK(strcmp(tmp_val.val.s, "foo\\\"bar\\\" ") == 0);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("Reading lists to values works") {
	//test one element lists
	strncpy(buf, "[\"foo\"]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.l[0].type == VAL_STR);
	CHECK(strcmp(tmp_val.val.l[0].val.s, "foo") == 0);
	spcl_cleanup_val(&tmp_val);
	//test two element lists
	strncpy(buf, "[\"foo\", 1]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 2);
	CHECK(tmp_val.val.l[0].type == VAL_STR);
	CHECK(strcmp(tmp_val.val.l[0].val.s, "foo") == 0);
	CHECK(tmp_val.val.l[1].type == VAL_NUM);
	CHECK(tmp_val.val.l[1].val.x == 1);
	spcl_cleanup_val(&tmp_val);
	//test lists of lists
	strncpy(buf, "[[1,2,3], [\"4\", \"5\", \"6\"]]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 2);
	{
	    //check the first sublist
	    value element = tmp_val.val.l[0];
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
	spcl_cleanup_val(&tmp_val);
	//test lists interpretations
	strncpy(buf, "[[i*2 for i in range(2)], [i*2-1 for i in range(1,3)], [x for x in range(1,3,0.5)]]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.val.l != NULL);
	CHECK(tmp_val.n_els == 3);
	{
	    //check the first sublist
	    value element = tmp_val.val.l[0];
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
	spcl_cleanup_val(&tmp_val);
	//test nested list interpretations
	strncpy(buf, "[[x*y for x in range(1,6)] for y in range(5)]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
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
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("Reading vectors to values works") {
	//test one element lists
	strncpy(buf, "vec(1.2, 3.4,56.7)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.val.a != NULL);
	CHECK(tmp_val.val.a[0] == doctest::Approx(1.2));
	CHECK(tmp_val.val.a[1] == doctest::Approx(3.4));
	CHECK(tmp_val.val.a[2] == doctest::Approx(56.7));
	spcl_cleanup_val(&tmp_val);

	strncpy(buf, "array([1.2, 3.4,56.7])", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.val.a != NULL);
	CHECK(tmp_val.val.a[0] == doctest::Approx(1.2));
	CHECK(tmp_val.val.a[1] == doctest::Approx(3.4));
	CHECK(tmp_val.val.a[2] == doctest::Approx(56.7));
	spcl_cleanup_val(&tmp_val);

	strncpy(buf, "array([[0, 1, 2], [3, 4, 5], [6, 7, 8]])", BUF_SIZE);buf[BUF_SIZE-1] = 0;
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
	spcl_cleanup_val(&tmp_val);
    }
    destroy_spcl_inst(sc);
}

/*TEST_CASE("function parsing") {
    char buf[BUF_SIZE];

    const char* test_func_1 = "f()";
    const char* test_func_2 = "f(\"a\", \"b\", \"c\", 4)";
    const char* test_func_3 = "foo(vec(1,2,3),\"a\",\"banana\")";
    const char* test_func_4 = "foo(1, \"box(0,1,2,3)\", 4+5)";
    const char* test_func_5 = "foo ( 1 , \"b , c\" )";
    const char* test_func_6 = "f(eps = 3.5)";
    const char* test_func_7 = "f(name = \"bar\")";
    const char* test_func_8 = "f(a, b1, c2)";
    const char* bad_test_func_1 = "foo ( a , b , c";
    const char* bad_test_func_2 = "foo ( \"a\" , \"b\" , \"c\"";

    spcl_inst* sc = make_spcl_inst(NULL);
    value er = spcl_make_none();
    //check string 1
    strncpy(buf, test_func_1, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    spcl_func_call cur_func = parse_func(sc, buf, 1, &er, NULL, 0);
    CHECK(er.type != VAL_ERR);
    CHECK(cur_func.n_args == 0);
    CHECK(strcmp(cur_func.name, "f") == 0);
    cleanup_func(&cur_func);
    //check string 2
    strncpy(buf, test_func_2, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 1, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 4);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "f") == 0);
    INFO("func arg=", cur_func.args[0]);
    CHECK(strcmp(cur_func.args[0].val.val.s, "a") == 0);
    INFO("func arg=", cur_func.args[1]);
    CHECK(strcmp(cur_func.args[1].val.val.s, "b") == 0);
    INFO("func arg=", cur_func.args[2]);
    CHECK(strcmp(cur_func.args[2].val.val.s, "c") == 0);
    CHECK(cur_func.args[3].val.val.x == 4);
    cleanup_func(&cur_func);
    //check string 3
    strncpy(buf, test_func_3, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 3, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 3);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "foo") == 0);
    INFO("func arg=", cur_func.args[0]);
    CHECK(cur_func.args[0].val.type == VAL_ARRAY);
    double* tmp_vec = cur_func.args[0].val.val.a;
    CHECK(tmp_vec[0] == 1.0);
    CHECK(tmp_vec[1] == 2.0);
    CHECK(tmp_vec[2] == 3.0);
    INFO("func arg=", cur_func.args[1].val.val.s);
    CHECK(strcmp(cur_func.args[1].val.val.s, "a") == 0);
    INFO("func arg=", cur_func.args[1]);
    CHECK(strcmp(cur_func.args[2].val.val.s, "banana") == 0);
    cleanup_func(&cur_func);
    //check string 4
    strncpy(buf, test_func_4, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 3, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 3);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "foo") == 0);
    INFO("func arg=", cur_func.args[0].val.val.x);
    CHECK(cur_func.args[0].val.val.x == 1);
    INFO("func arg=", cur_func.args[1].val.val.s);
    CHECK(strcmp(cur_func.args[1].val.val.s, "box(0,1,2,3)") == 0);
    INFO("func arg=", cur_func.args[2].val.val.x);
    CHECK(cur_func.args[2].val.val.x == 9);
    cleanup_func(&cur_func);
    //check string 5
    strncpy(buf, test_func_5, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 4, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 2);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "foo") == 0);
    INFO("func arg=", cur_func.args[0].val.val.x);
    CHECK(cur_func.args[0].val.val.x == 1);
    INFO("func arg=", cur_func.args[1].val.val.s);
    CHECK(strcmp(cur_func.args[1].val.val.s, "b , c") == 0);
    cleanup_func(&cur_func);
    //check string 6
    strncpy(buf, test_func_6, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 1, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 1);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "f") == 0);
    INFO("func arg=", cur_func.args[0].val.val.x);
    CHECK(cur_func.args[0].val.val.x == 3.5);
    CHECK(strcmp(cur_func.args[0].name, "eps") == 0);
    cleanup_func(&cur_func);
    //check string 7
    strncpy(buf, test_func_7, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 1, &er, NULL, 0);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 1);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "f") == 0);
    INFO("func arg=", cur_func.args[0].val.val.s);
    CHECK(strcmp(cur_func.args[0].val.val.s, "bar") == 0);
    CHECK(strcmp(cur_func.args[0].name, "name") == 0);
    cleanup_func(&cur_func);
    //check string 8 (function declaration parsing)
    strncpy(buf, test_func_8, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 1, &er, NULL, true);
    REQUIRE(er.type != VAL_ERR);
    REQUIRE(cur_func.n_args == 3);
    INFO("func name=", cur_func.name);
    CHECK(strcmp(cur_func.name, "f") == 0);
    CHECK(strcmp(cur_func.args[0].name, "a") == 0);
    CHECK(strcmp(cur_func.args[1].name, "b1") == 0);
    CHECK(strcmp(cur_func.args[2].name, "c2") == 0);
    cleanup_func(&cur_func);
    //check bad string 1
    strncpy(buf, bad_test_func_1, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 4, &er, NULL, 0);
    CHECK(er.type == VAL_ERR);
    CHECK(er.val.e->c == E_BAD_SYNTAX);
    spcl_cleanup_val(&er);
    cleanup_func(&cur_func);
    //check bad string 2
    strncpy(buf, bad_test_func_2, BUF_SIZE);buf[BUF_SIZE-1] = 0;
    cur_func = parse_func(sc, buf, 4, &er, NULL, 0);
    CHECK(er.type == VAL_ERR);
    CHECK(er.val.e->c == E_BAD_SYNTAX);
    spcl_cleanup_val(&er);
    cleanup_func(&cur_func);
    destroy_spcl_inst(sc);
}*/

TEST_CASE("string handling") {
    const size_t STR_SIZE = 64;
    spcl_inst* sc = make_spcl_inst(NULL);
    char buf[STR_SIZE];memset(buf, 0, STR_SIZE);
    //check that parsing an empty (or all whitespace) string gives VAL_UNDEF
    value tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    spcl_cleanup_val(&tmp_val);
    strncpy(buf, " ", STR_SIZE);
    tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    spcl_cleanup_val(&tmp_val);
    strncpy(buf, "\t  \t\n", STR_SIZE);
    tmp_val = spcl_parse_line(sc, buf);
    CHECK(tmp_val.type == VAL_UNDEF);
    CHECK(tmp_val.n_els == 0);
    spcl_cleanup_val(&tmp_val);
    //now check that strings are the right length
    for (size_t i = 1; i < STR_SIZE-1; ++i) {
	for (size_t j = 0; j < i; ++j)
	    buf[j] = 'a';
	buf[0] = '\"';
	buf[i] = '\"';
	buf[i+1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.n_els == i);
	spcl_cleanup_val(&tmp_val);
	CHECK(tmp_val.n_els == 0);
    }
    destroy_spcl_inst(sc);
}

TEST_CASE("operations") {
    spcl_inst* sc = make_spcl_inst(NULL);
    char buf[BUF_SIZE];
    SUBCASE("Arithmetic works") {
        //single operations
        strncpy(buf, "1+1.1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        value tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.1);
        strncpy(buf, "2-1.25", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 0.75);
        strncpy(buf, "2*1.1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.2);
        strncpy(buf, "2.2/2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.1);
        //order of operations
        strncpy(buf, "1+3/2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.5);
        strncpy(buf, "(1+3)/2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 2.0);
        strncpy(buf, "2*9/4*3", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 1.5);
    }
    SUBCASE("Comparisons work") {
	//create a single true and false, this makes things easier
        strncpy(buf, "2 == 2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "1 == 2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "[2, 3] == [2, 3]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "[2, 3, 4] == [2, 3]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "[2, 3, 4] == [2, 3, 5]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(!spcl_true(tmp_val));
        strncpy(buf, "\"apple\" == \"apple\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(spcl_true(tmp_val));
        strncpy(buf, "\"apple\" == \"banana\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type != VAL_ERR);
        CHECK(!spcl_true(tmp_val));
    }
    SUBCASE("String concatenation works") {
        //single operations
        strncpy(buf, "\"foo\"+\"bar\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        value tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_STR);
        CHECK(strcmp(tmp_val.val.s, "foobar") == 0);
	CHECK(tmp_val.n_els == 7);
        spcl_cleanup_val(&tmp_val);
	CHECK(tmp_val.n_els == 0);
    }
    SUBCASE("comparisons work") {
	//equality
	strncpy(buf, "1 == 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "1 == 2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	//geq
	strncpy(buf, "1 >= 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(spcl_true(tmp));
	//greater than
	strncpy(buf, "1 > 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "-1 > 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	//leq
	strncpy(buf, "1 <= 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//less than
	strncpy(buf, "1 < 2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "1 < 1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "1 < -1", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
    }
    SUBCASE("boolean operations work") {
	//or
	strncpy(buf, "false || false", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true || false", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "false || true", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "true || true", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//and
	strncpy(buf, "false && false", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true && false", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "false && true", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
	strncpy(buf, "true && true", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	//not
	strncpy(buf, "!false", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(spcl_true(tmp));
	strncpy(buf, "!true", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp = spcl_parse_line(sc, buf);
	CHECK(!spcl_true(tmp));
    }
    SUBCASE("Ternary operators work") {
	strncpy(buf, "(false) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(true) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	strncpy(buf, "(1 == 2) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(2 == 2) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	strncpy(buf, "(1 > 2) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 200);
	strncpy(buf, "(1 < 2) ? 100 : 200", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	test_num(tmp_val, 100);
	strncpy(buf, "(1 < 2) ? 100", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "expected \':\' in ternary") == 0);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("Missing end graceful failure") {
	strncpy(buf, "[1,2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	REQUIRE(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "expected ]") == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "a(1,2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "expected )") == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "\"1,2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "expected \"") == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "1,2]", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "unexpected ]") == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "1,2)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "unexpected )") == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "1,2\"", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_SYNTAX);
	INFO("message=", tmp_val.val.e->msg);
	CHECK(strcmp(tmp_val.val.e->msg, "expected \"") == 0);
	spcl_cleanup_val(&tmp_val);
    }
    destroy_spcl_inst(sc);
}

TEST_CASE("builtin functions") {
    char buf[BUF_SIZE];
    value tmp_val;
    spcl_inst* sc = make_spcl_inst(NULL);

    SUBCASE("range()") {
	strncpy(buf, "range(4)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 4);
	for (size_t i = 0; i < 4; ++i)
	    CHECK(tmp_val.val.a[i] == i);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "range(1,4)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 3);
	for (size_t i = 0; i < 3; ++i)
	    CHECK(tmp_val.val.a[i] == i+1);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "range(1,4,0.5)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_ARRAY);
	CHECK(tmp_val.n_els == 6);
	for (size_t i = 0; i < 6; ++i)
	    CHECK(tmp_val.val.a[i] == 0.5*i+1);
	spcl_cleanup_val(&tmp_val);
        //graceful failure cases
        strncpy(buf, "range()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	//check that divisions by zero are avoided
        strncpy(buf, "range(0,1,0)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_VALUE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "range(\"1\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "range(0.5,\"1\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "range(0.5,1,\"2\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("linspace()") {
        strncpy(buf, "linspace(1,2,5)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_ARRAY);
        CHECK(tmp_val.n_els == 5);
        for (size_t i = 0; i < 4; ++i) {
            CHECK(tmp_val.val.a[i] == 1.0+0.25*i);
        }
        spcl_cleanup_val(&tmp_val);
        strncpy(buf, "linspace(2,1,5)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
        CHECK(tmp_val.type == VAL_ARRAY);
        CHECK(tmp_val.n_els == 5);
        for (size_t i = 0; i < 4; ++i) {
            CHECK(tmp_val.val.a[i] == 2.0-0.25*i);
        }
        spcl_cleanup_val(&tmp_val);
        //graceful failure cases
        strncpy(buf, "linspace(2,1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "linspace(2,1,1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_VALUE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "linspace(\"2\",1,1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "linspace(2,\"1\",1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
        strncpy(buf, "linspace(2,1,\"1\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
        tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
        CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("flatten()") {
	strncpy(buf, "flatten([])", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 0);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "flatten([1,2,3])", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 3);
	for (size_t i = 0; i < 3; ++i) {
	    test_num(tmp_val.val.l[i], i+1);
	}
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "flatten([[1,2,3],[4,5],6])", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_LIST);
	CHECK(tmp_val.n_els == 6);
	for (size_t i = 0; i < 6; ++i)
	    test_num(tmp_val.val.l[i], i+1);

	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("math functions") {
	strncpy(buf, "math.sin(3.1415926535/2)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.sin(3.1415926535/6)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(0.5));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.cos(3.1415926535/2)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(0.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.cos(3.1415926535/6)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.sqrt(3)/2", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	value sqrt_val = spcl_parse_line(sc, buf);
	CHECK(sqrt_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(sqrt_val.val.x));
	strncpy(buf, "math.tan(3.141592653589793/4)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	strncpy(buf, "math.tan(0)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.n_els == 1);
	CHECK(tmp_val.val.x == doctest::Approx(0.0));
	strncpy(buf, "math.exp(0)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(1.0));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.exp(1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == doctest::Approx(2.718281828));
	CHECK(tmp_val.n_els == 1);
	strncpy(buf, "math.log(1)", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	CHECK(tmp_val.type == VAL_NUM);
	CHECK(tmp_val.val.x == 0);
	CHECK(tmp_val.n_els == 1);
	//failure conditions
	strncpy(buf, "math.sin()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.cos()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.tan()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.exp()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.sqrt()", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_LACK_TOKENS);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.sin(\"a\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.cos(\"a\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.tan(\"a\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.exp(\"a\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
	strncpy(buf, "math.sqrt(\"a\")", BUF_SIZE);buf[BUF_SIZE-1] = 0;
	tmp_val = spcl_parse_line(sc, buf);
	REQUIRE(tmp_val.type == VAL_ERR);
	CHECK(tmp_val.val.e->c == E_BAD_TYPE);
	spcl_cleanup_val(&tmp_val);
    }
    SUBCASE("assertions") {
	strncpy(buf, "assert(1)", BUF_SIZE);
	value tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	CHECK(tmp.n_els == 1);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(true)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(false)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(1 <= 3)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x != 0);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(1 == 1)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x != 0);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(1 > 3, \"1 is not greater than 3\")", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	CHECK(strcmp(tmp.val.e->msg, "1 is not greater than 3") == 0);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "isdef(apple)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 0);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "isdef(math.pi)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_NUM);
	CHECK(tmp.val.x == 1);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(apple)", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_UNDEF);
	spcl_cleanup_val(&tmp);
	strncpy(buf, "assert(isdef(apple))", BUF_SIZE);
	tmp = spcl_parse_line(sc, buf);
	CHECK(tmp.type == VAL_ERR);
	WARN(tmp.val.e->c == E_ASSERT);
	spcl_cleanup_val(&tmp);
    }
    destroy_spcl_inst(sc);
}

char* fetch_lb_line(const spcl_line_buffer* lb, size_t line, size_t off) {
    lbi s = make_lbi(line,off);
    lbi e = make_lbi(line, lb->line_sizes[line]);
    return lb_get_line(lb, s, e, NULL);
}

void write_test_file(const char** lines, size_t n_lines, const char* fname) {
    FILE* f = fopen(fname, "w");
    for (size_t i = 0; i < n_lines; ++i)
	fprintf(f, "%s\n", lines[i]);
    fclose(f);
}

TEST_CASE("test spcl_line_buffer it_single") {
    const char* lines[] = { "def test_fun(a)", "{", "if a > 5 {", "return 1", "}", "return 0", "}" };
    size_t n_lines = sizeof(lines)/sizeof(char*);
    write_test_file(lines, n_lines, TEST_FNAME);
    //check the lines (curly brace on new line)
    spcl_line_buffer* lb = make_spcl_line_buffer(TEST_FNAME);
    //test it_single
    int depth = 0;
    char* it_single_str = NULL;
    //including braces
    for (size_t i = 0; i < n_lines; ++i) {
	lbi start = make_lbi(i, 0);
	lbi end = make_lbi(i, 0);
	int it_start = it_single(lb, &it_single_str, '{', '}', &start, &end, &depth, true, false);
	if (i == 1)
	    CHECK(it_start == 0);
	else
	    CHECK(it_start == -1);
	CHECK(strcmp(it_single_str, lines[i]) == 0);
	CHECK(start.line == i);
	CHECK(start.off == 0);
	if (i < n_lines-1)
	    CHECK(end.line == start.line+1);
	else
	    CHECK(end.line == start.line);
	CHECK(end.off == strlen(lines[i]));
	if (i == 0 or i == n_lines-1)
	    CHECK(depth == 0);
	else
	    CHECK(depth >= 1);
    }
    //excluding braces
    const char* lines_exc[] = { "def test_fun(a)", "", "if a > 5 {", "return 1", "}", "return 0", "}" };
    for (size_t i = 0; i < n_lines; ++i) {
	lbi start = make_lbi(i, 0);
	lbi end = make_lbi(i, 0);
	int it_start = it_single(lb, &it_single_str, '{', '}', &start, &end, &depth, false, false);
	if (i == 1)
	    CHECK(it_start == 0);
	else
	    CHECK(it_start == -1);
	CHECK(strcmp(it_single_str, lines_exc[i]) == 0);
	//check that lines start where expected
	CHECK(start.line == i);
	if (i == 1)
	    CHECK(start.off == 1);
	else
	    CHECK(start.off == 0);
	if (i < n_lines-1)
	    CHECK(end.line == start.line+1);
	else
	    CHECK(end.line == start.line);
	//check that lines end where expected
	if (i == n_lines-1)
	    CHECK(end.off == 0);
	else
	    CHECK(end.off == strlen(lines[i]));
	//CHECK that the depths are correct
	if (i == 0 or i == n_lines-1)
	    CHECK(depth == 0);
	else
	    CHECK(depth >= 1);
    }
    destroy_spcl_line_buffer(lb);
}

TEST_CASE("spcl_line_buffer lb_get_enclosed") {
    const char* fun_contents[] = {"", "if a > 5 {", "return 1", "}", "return 0", ""};
    const char* if_contents[] = {"", "return 1", ""};
    size_t fun_n = sizeof(fun_contents)/sizeof(char*);
    size_t if_n = sizeof(if_contents)/sizeof(char*);

    lbi end_ind;
    char* strval = NULL;

    SUBCASE("open brace on a different line") {
	const char* lines[] = { "def test_fun(a)", "{", "if a > 5 {", "return 1", "}", "return 0", "}" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	//check the lines (curly brace on new line)
	spcl_line_buffer* lb = make_spcl_line_buffer(TEST_FNAME);
	for (size_t i = 0; i < n_lines; ++i) {
	    strval = fetch_lb_line(lb, i, 0);
	    CHECK(strcmp(lines[i], strval) == 0);
	    free(strval);
	}
	//test wrapper functions that use it_single
	lbi bstart;
	spcl_line_buffer* b_fun_con = lb_get_enclosed(lb, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_fun_con->n_lines == fun_n);
	CHECK(end_ind.line == 6);
	for (size_t i = 0; i < fun_n; ++i) {
	    strval = fetch_lb_line(b_fun_con, i,0);CHECK(strcmp(fun_contents[i], strval) == 0);free(strval);
	}
	spcl_line_buffer* b_if_con = lb_get_enclosed(b_fun_con, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_if_con->n_lines == if_n);
	CHECK(end_ind.line == 3);
	for (size_t i = 0; i < if_n; ++i) {
	    strval = fetch_lb_line(b_if_con, i,0);CHECK(strcmp(if_contents[i], strval) == 0);free(strval);
	}
	//check jumping
	lbi blk_start = make_lbi(0,0);
	lbi blk_end_ind = lb_jmp_enclosed(lb, blk_start, '{', '}', 0);
	CHECK(blk_end_ind.line == 6);
	CHECK(blk_end_ind.off == 0);
	blk_end_ind = lb_jmp_enclosed(lb, blk_start, '{', '}', true);
	CHECK(blk_end_ind.line == 6);
	CHECK(blk_end_ind.off == 1);
	//try flattening
	size_t len;
	char* fun_flat = lb_flatten(b_fun_con, 0, &len);
	CHECK(strcmp(fun_flat, "if a > 5 {return 1}return 0") == 0);
	CHECK(len == 28);
	free(fun_flat);
	fun_flat = lb_flatten(b_fun_con, '|', &len);
	CHECK(strcmp(fun_flat, "if a > 5 {|return 1|}|return 0||") == 0);
	CHECK(len == 33);
	free(fun_flat);
	destroy_spcl_line_buffer(lb);
	destroy_spcl_line_buffer(b_fun_con);
	destroy_spcl_line_buffer(b_if_con);
    }

    SUBCASE("open brace on the same line") {
	const char* lines[] = { "def test_fun(a) {", "if a > 5 {", "return 1", "}", "return 0", "}" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	//check the lines (curly brace on same line)
	spcl_line_buffer* lb = make_spcl_line_buffer(TEST_FNAME);
	for (size_t i = 0; i < n_lines; ++i) {
	    strval = fetch_lb_line(lb, i,0);CHECK(strcmp(lines[i], strval) == 0);free(strval);
	}
	//test wrapper functions that use it_single
	lbi bstart;
	spcl_line_buffer* b_fun_con = lb_get_enclosed(lb, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_fun_con->n_lines == fun_n);
	CHECK(end_ind.line == 5);
	for (size_t i = 0; i < fun_n; ++i) {
	    strval = fetch_lb_line(b_fun_con, i,0);CHECK(strcmp(fun_contents[i], strval) == 0);free(strval);
	}
	spcl_line_buffer* b_if_con_2 = lb_get_enclosed(b_fun_con, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_if_con_2->n_lines == if_n);
	CHECK(end_ind.line == 3);
	for (size_t i = 0; i < if_n; ++i) {
	    strval = fetch_lb_line(b_if_con_2, i,0);CHECK(strcmp(if_contents[i], strval) == 0);free(strval);
	}
	destroy_spcl_line_buffer(b_if_con_2);
	//check jumping
	lbi blk_start = make_lbi(0,0);
	lbi blk_end_ind = lb_jmp_enclosed(b_fun_con, blk_start, '{', '}', 0);
	CHECK(blk_end_ind.line == 3);
	CHECK(blk_end_ind.off == 0);
	blk_end_ind = lb_jmp_enclosed(b_fun_con, blk_start, '{', '}', true);
	CHECK(blk_end_ind.line == 3);
	CHECK(blk_end_ind.off == 1);
	//try flattening
	size_t len;
	char* fun_flat = lb_flatten(b_fun_con, 0, &len);
	CHECK(strcmp(fun_flat, "if a > 5 {return 1}return 0") == 0);
	CHECK(len == 28);
	free(fun_flat);
	fun_flat = lb_flatten(b_fun_con, '|', &len);
	CHECK(strcmp(fun_flat, "if a > 5 {|return 1|}|return 0||") == 0);
	CHECK(len == 33);
	free(fun_flat);
	destroy_spcl_line_buffer(lb);
	destroy_spcl_line_buffer(b_fun_con);
    }

    SUBCASE("everything on one line") {
	const char* lines[] = { "def test_fun(a) {if a > 5 {return 1}return 0}" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	//check the lines (curly brace on new line)
	spcl_line_buffer* lb = make_spcl_line_buffer(TEST_FNAME);
	for (size_t i = 0; i < n_lines; ++i) {
	    strval = fetch_lb_line(lb, i,0);CHECK(strcmp(lines[i], strval) == 0);free(strval);
	}
	//test wrapper functions that use it_single
	lbi bstart;
	spcl_line_buffer* b_fun_con = lb_get_enclosed(lb, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_fun_con->n_lines == 1);
	CHECK(end_ind.line == 0);
	strval = fetch_lb_line(b_fun_con, 0,0);CHECK(strcmp("if a > 5 {return 1}return 0", strval) == 0);free(strval);
	spcl_line_buffer* b_if_con = lb_get_enclosed(b_fun_con, bstart, &end_ind, '{', '}', 0, 0);
	CHECK(b_if_con->n_lines == 1);
	CHECK(end_ind.line == 0);
	strval = fetch_lb_line(b_if_con, 0,0);CHECK(strcmp("return 1", strval) == 0);free(strval);
	//check jumping
	lbi blk_start;
	lbi blk_end_ind = lb_jmp_enclosed(b_fun_con, blk_start, '{', '}', 0);
	CHECK(blk_end_ind.line == 0);
	CHECK(blk_end_ind.off == 18);
	blk_end_ind = lb_jmp_enclosed(b_fun_con, blk_start, '{', '}', true);
	CHECK(blk_end_ind.line == 0);
	CHECK(blk_end_ind.off == 19);
	//try flattening
	size_t len;
	char* fun_flat = lb_flatten(b_fun_con, 0, &len);
	CHECK(strcmp(fun_flat, "if a > 5 {return 1}return 0") == 0);
	CHECK(len == 28);
	free(fun_flat);
	fun_flat = lb_flatten(b_fun_con, '|', &len);
	CHECK(strcmp(fun_flat, "if a > 5 {return 1}return 0|") == 0);
	CHECK(len == 29);
	free(fun_flat);
	destroy_spcl_line_buffer(lb);
	destroy_spcl_line_buffer(b_fun_con);
	destroy_spcl_line_buffer(b_if_con);
    }
}

value test_fun_call(spcl_inst* c, spcl_func_call f) {
    value ret;
    if (f.n_args < 1)
	return spcl_make_err(E_LACK_TOKENS, "expected 1 argument");
    if (f.args[0].v.type != VAL_NUM)
	return spcl_make_err(E_LACK_TOKENS, "only works with numbers");
    double a = f.args[0].v.val.x;
    if (a > 5) {
	ret.type = VAL_INST;
	ret.val.c = make_spcl_inst(c);
	spcl_set_value(ret.val.c, "name", spcl_make_str("hi"), 0);
	return ret;
    }
    return f.args[0].v;
}

value test_fun_gamma(spcl_inst* c, spcl_func_call f) {
    if (f.n_args < 1)
	return spcl_make_err(E_LACK_TOKENS, "expected 1 argument");
    if (f.args[0].v.type != VAL_NUM)
	return spcl_make_err(E_LACK_TOKENS, "only works with numbers");
    double a = f.args[0].v.val.x;
    return spcl_make_num(sqrt(1 - a*a));
}

TEST_CASE("spcl_inst lookups") {
    const char* letters = "etaoin";
    size_t n_letters = strlen(letters);
    const size_t GEN_LEN = 4;
    char name[GEN_LEN+1];
    memset(name, 0, GEN_LEN+1);
    spcl_inst* c = make_spcl_inst(NULL);
    spcl_set_value(c, "tao", spcl_make_str("tao"), 0);
    size_t n_combs = 1;
    for (size_t i = 0; i < GEN_LEN; ++i)
	n_combs *= n_letters;

    //add a whole bunch of words using the most common letters
    value v;
    size_t before_size = c->n_memb;
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < n_combs; ++i) {
	size_t j = i;
	size_t k = 0;
	do {
	    name[k++] = letters[j % n_letters];
	    j /= n_letters;
	} while (j && k < GEN_LEN);
	spcl_set_value(c, name, spcl_make_num(i), 1);
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
	value er;
	spcl_inst* c = spcl_inst_from_file(TEST_FNAME, &er, 0, NULL);
	CHECK(er.type != VAL_ERR);
	//lookup the named values
	value val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_NUM);
	CHECK(val_a.val.x == 1);
	value val_c = spcl_find(c, "c");
	CHECK(val_c.type == VAL_LIST);
	CHECK(val_c.n_els == 2);
	CHECK(val_c.val.l[0].type == VAL_STR);
	CHECK(val_c.val.l[1].type == VAL_STR);
	destroy_spcl_inst(c);
    }
    SUBCASE ("with nesting") {
	const char* lines[] = { "a = {name = \"apple\",", "values = [20, 11]}", "b = a.values[0]", "c = a.values[1] + a.values[0]+1" }; 
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	value er;
	spcl_inst* c = spcl_inst_from_file(TEST_FNAME, &er, 0, NULL);
	CHECK(er.type != VAL_ERR);
	//lookup the named values
	value val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_INST);
	value val_a_name = spcl_find(val_a.val.c, "name");
	CHECK(val_a_name.type == VAL_STR);
	CHECK(strcmp(val_a_name.val.s, "apple") == 0);
	value val_a_value = spcl_find(val_a.val.c, "values");
	CHECK(val_a_value.type == VAL_LIST);
	CHECK(val_a_value.n_els == 2);
	CHECK(val_a_value.val.l[0].type == VAL_NUM);
	CHECK(val_a_value.val.l[1].type == VAL_NUM);
	CHECK(val_a_value.val.l[0].val.x == 20);
	CHECK(val_a_value.val.l[1].val.x == 11);
	value val_b = spcl_find(c, "b");
	CHECK(val_b.type == VAL_NUM);
	CHECK(val_b.val.x == 20);
	value val_c = spcl_find(c, "c");
	CHECK(val_c.type == VAL_NUM);
	CHECK(val_c.val.x == 32);
	destroy_spcl_inst(c);
    }
    SUBCASE ("user defined functions") {
	const char* fun_name = "test_fun";
	char* tmp_name = strdup(fun_name);

	const char* lines[] = { "a = test_fun(1)", "b=test_fun(10)" };
	size_t n_lines = sizeof(lines)/sizeof(char*);
	write_test_file(lines, n_lines, TEST_FNAME);
	spcl_line_buffer* b_1 = make_spcl_line_buffer(TEST_FNAME);
	spcl_inst* c = make_spcl_inst(NULL);
	value tmp_f = spcl_make_fn("test_fun", 1, &test_fun_call);
	spcl_set_value(c, "test_fun", tmp_f, 1);
	spcl_cleanup_val(&tmp_f);
	value er = spcl_read_lines(c, b_1);
	REQUIRE(er.type != VAL_ERR);
	//make sure that the function is there
	value val_fun = spcl_find(c, "test_fun");
	CHECK(val_fun.type == VAL_FUNC);
	//make sure that the number value a is there
	value val_a = spcl_find(c, "a");
	CHECK(val_a.type == VAL_NUM);
	CHECK(val_a.val.x == 1);
	value val_b = spcl_find(c, "b");
	CHECK(val_b.type == VAL_INST);
	value val_b_name = spcl_find(val_b.val.c, "name");
	CHECK(val_b_name.type == VAL_STR);
	CHECK(strcmp(val_b_name.val.s, "hi") == 0);
	free(tmp_name);
	destroy_spcl_inst(c);
	destroy_spcl_line_buffer(b_1);
    }
    SUBCASE ("stress test") {
	//first we add a bunch of arbitrary variables to make searching harder for the parser
	const char* lines1[] = {
	    "Vodkis=1","Pagne=2","Meadaj=3","whis=4","nac4=5","RaKi=6","gyn=7","cid=8","Daiqui=9","Mooshi=10","Magnac=2","manChe=3","tes=4","Bourbu=5","magna=6","sak=7","Para=8","Keffi=9","Guino=10","Uuqax=11","Thraxeods=12","Trinzoins=13","gheds=14","theSoild=15","vengirs=16",
	    "y = 2.0", "xs = linspace(0, y, 10000)", "arr1 = [math.sin(6*x/y) for x in xs]" };
	size_t n_lines1 = sizeof(lines1)/sizeof(char*);
	write_test_file(lines1, n_lines1, TEST_FNAME);
	spcl_line_buffer* b_1 = make_spcl_line_buffer(TEST_FNAME);
	const char* lines2[] = { "arr2 = [gam(x/y) for x in xs]" };
	size_t n_lines2 = sizeof(lines2)/sizeof(char*);
	write_test_file(lines2, n_lines2, TEST_FNAME);
	spcl_line_buffer* b_2 = make_spcl_line_buffer(TEST_FNAME);
	spcl_inst* c = make_spcl_inst(NULL);
	value er = spcl_read_lines(c, b_1);
	CHECK(er.type != VAL_ERR);
	value tmp_f = spcl_make_fn("gam", 1, &test_fun_gamma);
	spcl_set_value(c, "gam", tmp_f, 1);
	spcl_cleanup_val(&tmp_f);
	er = spcl_read_lines(c, b_2);
	CHECK(er.type != VAL_ERR);
	destroy_spcl_inst(c);
	destroy_spcl_line_buffer(b_1);
	destroy_spcl_line_buffer(b_2);
    }
}
static const valtype SRC_SIG[] = {VAL_STR, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_NUM, VAL_INST};
value spcl_gen_gaussian_source(spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck_opts(f, 6, SRC_SIG);
    /*value ret = check_signature(f, SIGLEN(SRC_SIG), SIGLEN(SRC_SIG)+3, SRC_SIG);
    if (ret.type)
	return ret;
    if (f.args[f.n_args-1].val.type != VAL_INST)
	return spcl_make_err(E_BAD_TYPE, "");*/
    value ret = spcl_make_inst(c, "Gaussian_source");
    spcl_set_value(ret.val.c, "component", f.args[0].v, 1);
    spcl_set_value(ret.val.c, "wavelength", f.args[1].v, 0);
    spcl_set_value(ret.val.c, "amplitude", f.args[2].v, 0);
    spcl_set_value(ret.val.c, "width", f.args[3].v, 0);
    spcl_set_value(ret.val.c, "phase", f.args[4].v, 0);
    //read additional parameters
    spcl_set_value(ret.val.c, "cutoff", (f.n_args>6)? f.args[5].v: spcl_make_num(5), 0);
    spcl_set_value(ret.val.c, "start_time", (f.n_args>7)? f.args[6].v: spcl_make_num(5), 0);
    spcl_set_value(ret.val.c, "region", f.args[f.n_args-1].v, 1);
    return ret;
}
static const valtype BOX_SIG[] = {VAL_ARRAY, VAL_ARRAY};
value spcl_gen_box(spcl_inst* c, spcl_func_call f) {
    spcl_sigcheck(f, BOX_SIG);
    /*value ret = check_signature(f, SIGLEN(BOX_SIG), SIGLEN(BOX_SIG)+3, BOX_SIG);
    if (ret.type)
	return ret;*/
    value ret = spcl_make_inst(c, "Box");
    spcl_set_value(ret.val.c, "pt_1", f.args[0].v, 1);
    spcl_set_value(ret.val.c, "pt_2", f.args[1].v, 1);
    return ret;
}
void setup_geometry_inst(spcl_inst* con) {
    //we have to set up the spcl_inst with all of our functions
    spcl_set_value(con, "Gaussian_source", spcl_make_fn("Gaussian_source", 6, &spcl_gen_gaussian_source), 0);
    spcl_set_value(con, "Box", spcl_make_fn("Box", 2, &spcl_gen_box), 0);
}

TEST_CASE("file parsing") {
    char buf[BUF_SIZE];
    spcl_line_buffer* lb = make_spcl_line_buffer("test.geom");
    spcl_inst* c = make_spcl_inst(NULL);
    setup_geometry_inst(c);
    value er = spcl_read_lines(c, lb);
    CHECK(er.type != VAL_ERR);
    value v = spcl_find(c, "offset");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 0.2);
    v = spcl_find(c, "list");
    CHECK(v.type == VAL_LIST);
    v = spcl_find(c, "sum_list");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 11);
    v = spcl_find(c, "prod_list");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 24.2);
    v = spcl_find(c, "acid_test");
    CHECK(v.type == VAL_NUM);CHECK(v.val.x == 16);
    v = spcl_find(c, "acid_res");
    CHECK(spcl_true(v));
    //lookup only does a shallow copy so we don't need to free

    strncpy(buf, "gs.__type__ == \"Gaussian_source\"", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.component == \"Ey\"", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.wavelength == 1.5", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.amplitude == 7", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.width == 3", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.phase == 0.75", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.cutoff == 6", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.start_time == 5.2", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.__type__ == \"Box\"", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.pt_1 == vec(0,0,.2)", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    strncpy(buf, "gs.region.pt_2 == vec(.4, 0.4, .2)", BUF_SIZE);
    CHECK(spcl_true(spcl_parse_line(c, buf)));
    destroy_spcl_line_buffer(lb);
    destroy_spcl_inst(c);
}
