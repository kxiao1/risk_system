#include "risk_system.h"

int main() {
    Log::print_test_name("Constructing a risk management system");

    // assume we debug from the build folder where the binary is placed
    RiskManagementSystem<G5> rms("../ref/rates.txt", "../ref/portfolio.txt");
#ifdef DEBUG
    rms.test_debug();
#endif
    using enum G5::Currency;
    Log::print_test_double(rms.get_discount_factor(EUR, 20).value());
    Log::print_test_double(rms.get_discount_factor(EUR, 30).value());
    Log::print_test_double(rms.get_discount_factor(EUR, 45).value());
    Log::print_test_double(rms.get_discount_factor(EUR, 360).value());
    Log::print_test_double(rms.get_discount_factor(EUR, 9999).value());
    Log::print_test_double(rms.get_discount_factor(CAD, 30).has_value());
    Log::print_test_double(rms.get_fx_spot({EUR, USD}).value());
    Log::print_test_double(rms.get_fx_spot({USD, JPY}).value());
    Log::print_test_double(rms.get_fx_spot({EUR, JPY}).value());
    Log::print_test_double(rms.get_fx_spot({GBP, EUR}).value());
    Log::print_test_double(rms.get_fx_spot({USD, USD}).value());
    Log::print_test_double(rms.get_fx_spot({USD, CAD}).has_value());

    // Be careful working with optionals! if get_tenor returns optional<vector>,
    // for (auto v: rms.get_tenors(USD).value()) {...} gives undefined behavior
    // https://stackoverflow.com/questions/29990045/temporary-lifetime-in-range-for-expression/29990971#29990971
    // See also the "Temporary range expression" section of
    // https://en.cppreference.com/w/cpp/language/range-for
    // a C++20 workaround for(<init>; auto&x: <init>.value()) {...} exists
    auto v1 = rms.get_tenors(USD);
    std::ranges::sort(v1);  // try out the ranges version
    Log::print_test_name("Tenors in days for USD:");
    Log::print_test_vector(v1);

    auto v2 = rms.get_tenors(CAD);
    std::ranges::sort(v2);  // try out the ranges version
    Log::print_test_name("Tenors in days for CAD:");
    Log::print_test_vector(v2);

    auto v3 = rms.get_maturities(EUR);
    std::ranges::sort(v3);  // try out the ranges version
    Log::print_test_name("EUR Maturities:");
    Log::print_test_vector(v3);

    double DV01_tenor{rms.get_DV01(USD, 360).value()};
    Log::print_test_name("DV01 for USD, tenor = 360 days:");
    Log::print_test_double(DV01_tenor);

    double DV01{rms.get_DV01(USD).value()};
    Log::print_test_name("DV01 for USD:");
    Log::print_test_double(DV01);
}