#include <iostream>
#include <cmath>
#include "result.hpp"

using namespace result;

int sqrt(int x)
{
    return int(std::sqrt(x));
}

Result<int, std::runtime_error> div_safe(int a, int b)
{
    if (b != 0) {
        return ok(a / b);
    } else {
        return err(std::runtime_error("Division by zero is undefined!"));
    }
}

Result<int, std::exception> div_sqrt(int a, int b)
{
    int value;
    TRY(div_safe(a, b), value);

    int root = sqrt(value);
    return ok(root);
}

int main()
{
    using namespace result;

    std::cout << "Hello, World!\n";
    
    int a, b;
    std::cin >> a >> b;

    auto result = div_sqrt(a, b);

    if (result.is_ok())
    {
        std::cout << result.unwrap();
    } else {
        std::cout << "Error: " << result.unwrap_err().what();
    }

    std::cout << std::endl;
    std::cin.get();

    return 0;
}
