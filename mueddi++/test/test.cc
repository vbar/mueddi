#include "acutest.h"
#include "mueddi.hh"

#include <vector>
#include <set>
#include <string>

using namespace mueddi;

void test_initial_final()
{
    const char *data[] = { "", "a" };

    std::vector<std::string> v;
    for (size_t i = 0; i < 2; ++i) {
        v.push_back(std::string(data[i]));
    }

    Dawg dawg = make_dawg(v);
    InputIterator it(std::string("b"), 1, dawg);
    std::set<std::string> res(it, InputIterator());

    TEST_CHECK(res.size() == 2);
    for (size_t i = 0; i < 2; ++i) {
        TEST_CHECK(res.find(std::string(data[i])) != res.end());
    }
}

void test_foo()
{
    const char *data[] = { "foo", "bar" };

    std::vector<std::string> v;
    for (size_t i = 0; i < 2; ++i) {
        v.push_back(std::string(data[i]));
    }

    Dawg dawg = make_dawg(v);
    InputIterator end;

    {
        InputIterator it(std::string("baz"), 1, dawg);
        std::set<std::string> res(it, end);
        TEST_CHECK(res.size() == 1);
        TEST_CHECK(*(res.begin()) == "bar");
    }

    {
        InputIterator it(std::string("baz"), 2, dawg);
        std::set<std::string> res(it, end);
        TEST_CHECK(res.size() == 1);
        TEST_CHECK(*(res.begin()) == "bar");
    }
}

void test_this()
{
    const char *data[] = { "this", "that", "other" };

    std::vector<std::string> v;
    for (size_t i = 0; i < 3; ++i) {
        v.push_back(std::string(data[i]));
    }

    Dawg dawg = make_dawg(v);
    InputIterator end;

    {
        InputIterator it(std::string("the"), 1, dawg);
        TEST_CHECK(it == end);
    }

    {
        InputIterator it(std::string("the"), 2, dawg);
        std::set<std::string> res(it, end);

        TEST_CHECK(res.size() == 3);
        for (size_t i = 0; i < 3; ++i) {
            TEST_CHECK(res.find(std::string(data[i])) != res.end());
        }
    }
}

void test_long_head()
{
    const char *single = "abtrbtz";

    std::vector<std::string> v;
    v.push_back(std::string(single));

    Dawg dawg = make_dawg(v);
    InputIterator it(std::string("abtrtz"), 1, dawg);
    std::set<std::string> res(it, InputIterator());
    TEST_CHECK(res.size() == 1);
    TEST_CHECK(*(res.begin()) == single);
}

void test_tolerance()
{
    const char *data[] = { "meter", "otter", "potter" };

    std::vector<std::string> v;
    for (size_t i = 0; i < 3; ++i) {
        v.push_back(std::string(data[i]));
    }

    Dawg dawg = make_dawg(v);
    InputIterator end;

    {
        InputIterator it(std::string("mutter"), 1, dawg);
        TEST_CHECK(it == end);
    }

    {
        InputIterator it(std::string("mutter"), 2, dawg);
        std::set<std::string> res(it, end);

        TEST_CHECK(res.size() == 3);
        for (size_t i = 0; i < 3; ++i) {
            TEST_CHECK(res.find(std::string(data[i])) != res.end());
        }
    }
}

void test_binary()
{
    const char *data[] = { "ababa", "babab" };

    std::vector<std::string> v;
    for (size_t i = 0; i < 2; ++i) {
        v.push_back(std::string(data[i]));
    }

    Dawg dawg = make_dawg(v);
    InputIterator it(std::string("abba"), 3, dawg);
    std::set<std::string> res(it, InputIterator());
    TEST_CHECK(res == std::set<std::string>(data, data + 2));
}

TEST_LIST = {
   { "initial_final", test_initial_final },
   { "foo", test_foo },
   { "this", test_this },
   { "long_head", test_long_head },
   { "tolerance", test_tolerance },
   { "binary", test_binary },
   { nullptr, nullptr }
};
