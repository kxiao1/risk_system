/*
    Simplified risk system:
    - Read a list of interest rates for various tenors and currencies
    - Use C++ regex to take only i/r data and partition by currency
    - Construct a piecewise linear yield curve for each currency (see ref pdf)
    - Calculate with finite difference the PV01 of a portfolio with respect to
        - a bump on one tenor in the yield curve of the portfolio's currency
        - a uniform shift of the whole curve
        * the two functions take in a currency and return the impact on
   positions in that currency converted into USD (by default)
        * positions are just cash flow notionals by dates (see portfolio.txt)
    - (TODO) Repeat the process with FX spot rates to determine the fx delta
    - (TODO) Handle 'FX Forward' trades (see ref pdf and data in portfolio2.txt)

    All ref data is found in risk_system_ref/ and comes from NUS FE5226 (see
    https://github.com/fecpp/minirisk), on which this project is based.

    Learning points:
    - I/O (Regex, getline, [io/string]stream, std::hex, colorful output...)
    - enum classes, keys_view, ranges_sort and other C++20 features
    - passing member functions as lambdas
    - the "finally" idiom
    - [[no_discard]], [[maybe_unused]]
    - (builds) how to use cmake
    - (recaps) simplified yield curve construction, central differences for DV01
    - (recaps) macros, modular design

    Recap of equivalence of closure and DFA definitions of regular languages:
    - (=>) we can show DFA closure properties, or eps-transition -> NFA -> DFA
    - (<=) Kleene's algorithm: similar to Bellman-Ford and Floyd-Warshall!

    Note the following errors that were encountered when developing with g++9:
    - [-fpermissive] to refer to a type member of a template parameter, use
   ‘CcyGroup::Currency’ (hence "typename CcyGroup::CcyGroup::Currency")
    - lack of support for "using enum"
    The second issue disappears when the code is compiled with g++11 or another
    more C++20 compliant compiler, but the first remains an issue despite the
    "down with typname" improvements: see
    https://stackoverflow.com/questions/64995233/why-using-a-templates-parameters-type-member-as-a-return-type-of-a-function-wo

    Reference on yield curve construction:
    https://www.soa.org/sections/financial-reporting/financial-reporting-newsletter/2022/february/fr-2022-02-perelman/
*/
#include <chrono>   // get days since 1900 etc
#include <fstream>  // file stream
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "risk_system_structs.h"  // already includes logger.h

// Definitions are placed in the header file as suggested by
// https://isocpp.org/wiki/faq/templates#separate-template-fn-defn-from-decl

template <typename CcyGroup>
    requires std::is_base_of_v<G5, CcyGroup>
class RiskManagementSystem {
   public:
    // using enum G5::Currency; // error: template arguments are dependent types

    RiskManagementSystem(const std::string& rates_path,
                         const std::string& portfolio_path) {
        std::ifstream in_rates{rates_path}, in_portfolio{portfolio_path};
        if (!check_data(in_rates, rates_path) ||
            !check_data(in_portfolio, portfolio_path)) {
            throw "Check file paths?";
        }
        std::string line;
        std::getline(in_rates, line);  // discard the first line starting with #

        // e.g. IR.2W.EUR 0.025 (with thanks to https://regex101.com/)
        std::regex rates_format{
            R"(^IR\.[[:digit:]]+[[:upper:]]\.[[:upper:]]{3}[[:blank:]](?:[[:digit:]]+\.)?[[:digit:]]+$)"};

        // e.g. FX.SPOT.EUR 1.1213 (always XXXUSD)
        std::regex fx_format{
            R"(^FX\.SPOT\.[[:upper:]]{3}[[:blank:]](?:[[:digit:]]+\.)?[[:digit:]]+$)"};

        // https://stackoverflow.com/questions/36213659/how-to-sscanf-in-c
        while (std::getline(in_rates, line)) {
            if (std::regex_search(line, rates_format)) {
                parse_rate(line);
                continue;
            }

            if (std::regex_search(line, fx_format)) {
                parse_fx(line);
                continue;
            }
            Log::warn_line(line);
        }

#ifdef DEBUG
        using namespace std::chrono;  // just for next two lines
        // sys_days is an alias to time_point<system_clock, days>
        // its arguments can also initialize a std::chrono::year_month_day
        sys_days epoch{January / 1 / 1900};  // C++20 operator/ syntax
        delta = static_cast<int>(
            (floor<days>(system_clock::now()) - epoch).count());
        Log::info_delta(delta);
#endif
        delta = 42940;  // strategically overriden to match the portfolio dates
        std::getline(in_portfolio, line);  // discard first line starting with #

        std::regex trade_format{
            R"(^[[:digit:]]+;[a-f0-9]{8};[[:upper:]]{3};[[:digit:]]{5};$)"};
        while (std::getline(in_portfolio, line)) {
            if (std::regex_search(line, trade_format)) {
                parse_trade(line);
                continue;
            }
            Log::warn_line(line);
        }
    }

    // Return an owning container rather than a non-owning view
    std::vector<int> get_maturities(CcyGroup::Currency ccy) {
        if (!check_maturities(ccy)) return {};
        Log::info_maturities(CcyGroup::to_string(ccy));
        auto k = currency_notionals.at(ccy).get_maturities();
        return std::vector<int>(k.begin(), k.end());
    }

    // To be crazy, we could return std::optional<view_type> where we define
    // using view_type =
    // std::ranges::keys_view<std::views::all_t<decltype(*InterestRates::r)>>;
    // Explanation of return type: https://cplusplus.github.io/LWG/issue3563
    // If we wanted to return an empty optional of the right type:
    // return std::optional<typename std::result_of<decltype (
    //     &InterestRates::get_tenors)(InterestRates)>::type>();
    // return std::optional<decltype(currency_rates.at(ccy).get_tenors())>();
    // return std::make_optional(currency_rates.at(ccy).get_tenors());
    // default-constructing a keys_view (return {}) somehow crashes in g++11
    // e.g. if (!check_rates(ccy)) return InterestRates{}.get_tenors();
    std::vector<int> get_tenors(CcyGroup::Currency ccy) {
        if (!check_rates(ccy)) return {};
        Log::info_tenors(CcyGroup::to_string(ccy));

        auto k = currency_rates.at(ccy).get_tenors();
        return std::vector<int>(k.begin(), k.end());
    }

    std::optional<double> get_discount_factor(CcyGroup::Currency ccy,
                                              int tenor) {
        if (!check_rates(ccy) || !check_tenor_val(tenor)) return {};
        Log::info_discounts(CcyGroup::to_string(ccy), tenor);

        return std::make_optional(
            currency_rates.at(ccy).get_discount_factor(tenor));
    }

    std::optional<double> get_fx_spot(
        std::pair<typename CcyGroup::Currency, typename CcyGroup::Currency>
            ccy_pair) {
        auto& [base, term] = ccy_pair;
        if (!check_fx(base) || !check_fx(term)) return {};
        Log::info_fx(CcyGroup::to_string(base), CcyGroup::to_string(term));

        return std::make_optional(currency_spot.at(base) /
                                  currency_spot.at(term));
    }

    // Get the DV01 in the desired ccy by bumping only one tenor
    std::optional<double> get_DV01(CcyGroup::Currency ccy, int tenor) {
        if (!check_tenor_rate(ccy, tenor) || !check_fx(ccy)) return {};
        Log::info_DV01(CcyGroup::to_string(ccy), tenor);

        auto& rates = currency_rates.at(ccy);
        auto& trades = currency_notionals.at(ccy);

        // bump the curve in its own scope so it gets unbumped when we exit
        auto get_bumped_value = [&rates, &trades, &tenor](double bump) {
            auto unbump_later = rates.bump_tenor(tenor, bump);
            auto discount_fn = [&rates](int tenor) {
                return rates.get_discount_factor(tenor);
            };
            return trades.get_book_value(discount_fn);
        };

        // Convert sensitivity to local rates to USD before returning
        return std::make_optional(
            get_fx_spot({CcyGroup::Currency::USD, ccy}).value() *
            // 2nd-order approx
            -(get_bumped_value(EPS) - get_bumped_value(-EPS)) / 2);
    }

    // Get the DV01 in the desired ccy by bumping the entire curve
    std::optional<double> get_DV01(CcyGroup::Currency ccy) {
        if (!check_rates(ccy) || !check_fx(ccy)) return {};
        Log::info_DV01(CcyGroup::to_string(ccy));

        auto& rates = currency_rates.at(ccy);
        auto& trades = currency_notionals.at(ccy);

        // Bump the curve in its own scope so it gets unbumped when we exit
        auto get_bumped_value = [&rates, &trades](double bump) {
            auto unbump_later = rates.bump_curve(bump);
            auto discount_fn = [&rates](int tenor) {
                return rates.get_discount_factor(tenor);
            };
            return trades.get_book_value(discount_fn);
        };

        // Convert sensitivity to local rates to USD before returning
        return std::make_optional(
            get_fx_spot({CcyGroup::Currency::USD, ccy}).value() *
            // 2nd-order approx
            -(get_bumped_value(EPS) - get_bumped_value(-EPS)) / 2);
    }

#ifdef DEBUG
    void test_debug() {
        // apparently this is safe
        [[maybe_unused]] int val = InterestRates{}.get_tenors().size();
        // this is also safe
        val = *InterestRates{}.getX();
        // this is not safe!
        // int* x = InterestRates{}.getX();
        // val = *x;
    }
#endif

   private:
    std::unordered_map<typename CcyGroup::Currency, InterestRates>
        currency_rates;
    std::unordered_map<typename CcyGroup::Currency, FXSpot> currency_spot{
        {CcyGroup::Currency::USD, {}}};
    std::unordered_map<typename CcyGroup::Currency, DateNotionals>
        currency_notionals;

    static constexpr double EPS{1e-4};  // or static inline
    int delta;  // no. of days from (Excel) 1900 epoch e.g. 4/29/2024 = 45410

    void parse_rate(const std::string& line) {
        Log::info_rate(line);
        std::istringstream s_line{line};
        std::string key_param;
        std::getline(s_line, key_param, '.');  // "IR", we ignore

        std::getline(s_line, key_param, '.');  // "2W"
        char name{key_param.back()};           // "W"
        key_param.pop_back();                  // "2"
        int tenor{std::stoi(key_param)};
        if (!check_tenor_val(tenor)) return;
        switch (name) {
            case 'D':
                tenor *= 1;
                break;
            case 'W':
                tenor *= 7;
                break;
            case 'M':
                tenor *= 30;
                break;
            case 'Y':
                tenor *= 360;
                break;
            default:
                Log::warn_tenor_char(tenor);
                return;
        }

        s_line >> key_param;  // "EUR"
        auto ccy_opt = CcyGroup::to_ccy(key_param);
        if (!ccy_opt) {
            Log::warn_ccy_str(key_param);
            return;
        }
        typename CcyGroup::Currency ccy = *ccy_opt;

        double rate;
        s_line >> rate;  // 0.025

        // we default-construct a InterestRates if this is the first data point
        currency_rates[ccy].add_rate(tenor, rate);
    }

    void parse_fx(const std::string& line) {
        Log::info_fx(line);
        std::istringstream s_line{line};
        std::string key_param;
        std::getline(s_line, key_param, '.');  // "FX", we ignore
        std::getline(s_line, key_param, '.');  // "SPOT", we ignore

        s_line >> key_param;  // "EUR"
        auto ccy_opt = CcyGroup::to_ccy(key_param);
        if (!ccy_opt) {
            Log::warn_ccy_str(key_param);
            return;
        }
        typename CcyGroup::Currency ccy = *ccy_opt;

        double spot;
        s_line >> spot;

        // default construct FXSpot object if necessary
        currency_spot[ccy].set_spot(spot);
    }

    void parse_trade(const std::string& line) {
        Log::info_trade(line);
        std::istringstream s_line{line};
        std::string key_param;
        std::getline(s_line, key_param, ';');  // id, we ignore

        std::getline(s_line, key_param, ';');  // notional
        int notional;
        std::istringstream{key_param} >> std::hex >> notional;

        std::getline(s_line, key_param, ';');  // ccy
        auto ccy_opt = CcyGroup::to_ccy(key_param);
        if (!ccy_opt) {
            Log::warn_ccy_str(key_param);
            return;
        }
        typename CcyGroup::Currency ccy = *ccy_opt;

        int payment_date;
        s_line >> payment_date;
        int tenor = payment_date - delta;
        if (!check_tenor_val(tenor)) return;
        Log::info_effective_tenor_notional(tenor, notional);

        if (!currency_notionals.contains(ccy)) {
            // default construct DateNotionals object
            currency_notionals[ccy].set_delta(delta);
        }
        currency_notionals.at(ccy).add_trade(payment_date, notional);
    }

    ///////////////////////// ERROR-CHECKING CODE /////////////////////////////
    bool check_data(const std::ifstream& in, const std::string& path) {
        if (!in) {
            Log::warn_data(path);
            return false;
        }
        return true;
    }
    bool check_rates(CcyGroup::Currency ccy) {
        if (!currency_rates.contains(ccy)) {
            Log::warn_rates(CcyGroup::to_string(ccy));
            return false;
        }
        return true;
    }
    bool check_maturities(CcyGroup::Currency ccy) {
        return currency_notionals.contains(ccy);
    }
    bool check_tenor_val(int tenor) {
        if (tenor < 0) {
            Log::warn_tenor_val(tenor);
            return false;
        }
        return true;
    }
    bool check_tenor_rate(CcyGroup::Currency ccy, int tenor) {
        if (!check_rates(ccy)) {
            return false;
        }
        if (!currency_rates.at(ccy).check_tenor(tenor)) {
            Log::warn_tenor_rate(CcyGroup::to_string(ccy), tenor);
            return false;
        }
        return true;
    }
    bool check_fx(CcyGroup::Currency ccy) {
        if (!currency_spot.contains(ccy)) {
            Log::warn_fx(CcyGroup::to_string(ccy));
            return false;
        }
        return true;
    }
};
