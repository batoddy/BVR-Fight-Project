#define main full_suite_main
#include "../src/otopilot/tests/units_test.cpp"
#undef main
int main()
{
    metric_wire_and_imperial_calculations();
    std::cout << "Pitbull export and IO checks passed.\n";
}
