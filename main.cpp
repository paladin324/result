#include <iostream>
#include <cmath>
#include <cassert>
#include "result.hpp"

using namespace result;

void run_all_tests();

int main()
{
    using namespace result;

    run_all_tests();
    std::cout << "All tests passed successfully!";

    std::cout << std::endl;
    std::cin.get();

    return 0;
}

struct unit {};

Result<int, unit> test_ok()
{
    return ok(10);
}

Result<unit, int> test_err()
{
    return err(10);
}

void test_is_ok_err()
{
    assert(ok(10).is_ok());
    assert(!ok(10).is_err());
    assert(err(10).is_err());
    assert(!err(10).is_ok());
}

void test_as_ref()
{
    Result<int, int> result = ok(10);
    const int& ok_val = result.as_ref().unwrap();
    assert(ok_val == 10);
}

void test_match()
{
    Result<int, int> result = err(10);
    result.match
    (
        [](int x)
        {
            assert(false);
        },
        [](int e)
        {
            assert(e == 10);
        }
    );
}

void test_map()
{
    auto square = [](int x) -> int { return x * x; };
    assert(ok(10).map(square).unwrap() == 100);
    assert(err(10).map_err(square).unwrap_err() == 100);
}

void test_eq()
{
    assert((ok<int, int>(10) == ok<int, int>(10)));
    assert((err<int, int>(10) == err<int, int>(10)));
    assert((ok<int, int>(10) != err<int, int>(10)));
}

void run_all_tests()
{
    test_ok();
    test_err();
    test_is_ok_err();
    test_as_ref();
    test_match();
    test_map();
    test_eq();
}
