#include <algorithm>
#include <array>
#include <cmath>  // std::exp
#include <functional>
#include <iostream>
#include <map>
#include <numeric>  //reduce
#include <optional>
#include <ranges>

#include "logger.h"
/*
    A base group of currencies on which we build our risk management system.
   What started out as unordered_maps became functions so that a group can be
   more explicitly inherited and overriden by subclasses (e.g. G10, EM...).
 */
struct G5 {
    // https://stackoverflow.com/questions/18335861/why-is-enum-class-considered-safer-to-use-than-plain-enum
    // https://stackoverflow.com/a/46401560
    enum class Currency { EUR, GBP, USD, CAD, JPY };  // we have EUR < GBP etc.

    static std::optional<Currency> to_ccy(const std::string& ccy_str) {
        // or std::find_if, std::distance...
        for (size_t i = 0; i < strings.size(); ++i) {
            if (ccy_str == strings[i]) {
                return std::make_optional(static_cast<Currency>(i));
            }
        }
        return {};
    }
    static std::string to_string(const Currency& ccy) {
        return strings[static_cast<int>(ccy)];
    }

    // Old method (with using enum Currency)
    // static inline std::unordered_map<std::string, Currency> currency_map{
    // {"EUR", EUR}, {"GBP", GBP}, {"USD", USD}, {"CAD", CAD}, {"JPY", JPY}};
    // static inline std::unordered_map<Currency, std::string> string_map{
    // {EUR, "EUR"}, {GBP, "GBP"}, {USD, "USD"}, {CAD, "CAD"}, {JPY, "JPY"}};

   private:
    // Most containers are expandable. Make sure the fixed size 5 is correct.
    // See also https://xuhuisun.com/post/c++-weekly-2-constexpr-map/ for a
    // fancier implementation. The "inline" keyword is needed otherwise strings
    // must be defined outside the class and in a cpp file:
    // https://www.learncpp.com/cpp-tutorial/static-member-variables/
    static inline const std::array<std::string, 5> strings{"EUR", "GBP", "USD",
                                                           "CAD", "JPY"};
};

/*
    Maintains and manipulates an interest rate curve. Notably, due to
    https://stackoverflow.com/questions/16766137/decltype-in-class-method-declaration-error-when-used-before-referenced-member
    (private) variables and classes that appear in the *return types* of other
    member functions must be declared first. So the private block comes first
    unless we abbreviate everything with auto.
*/
struct InterestRates {
    bool check_tenor(int tenor) { return rates.contains(tenor); }

    // The goal is to specify the return type as a view of the type of the rates
    // object, without specifying the object's type. For this, r must be
    // declared earlier. if this function is marked const, the expression
    // involving r must evaluate to const. Maybe there will be an alternative
    // method in C++23 using as_const?? Note the following doesn't work:
    // std::ranges::keys_view<std::views::all_t<std::map<int, double>>>
    // Need all_t<e> for some *expression* e (so need extra () for data member)
    // This works: std::ranges::keys_view<std::views::all_t<decltype(*r)>>
    auto get_tenors() const { return std::views::keys(rates); }

    // Suppose T_i, T_i+1, r_i, r_i+1 are given, and we want to find
    // exp(-r_t*t) for T_i <= t <= T_i+1. Here are two ways to interpolate:
    // 1) Linear spot interpolation:
    //  exp(-(r_i(T_i+1 - t) + r_i+1(t - T_i))t / (T_i+1 - T_i))
    // 2) Piecewise constant forwards:
    //  exp(-(r_iT_i(T_i+1 - t) + r_i+1T_i+1(t - T_i)) / (T_i+1 - T_i))
    // For simplicity we implement 1) as described in the doc.
    double get_discount_factor(int t) const {
        double r_left{0.0}, r_right{0.0};  // lower bound 0, upper bound r_N
        int t_left{0}, t_right{0};         // lower bound 0, upper bound t_N
        auto it = rates.upper_bound(t);    // avoid 0 = t_left = t = t_right
        if (it == rates.begin()) {
            t_right = it->first;
            r_right = it->second;
        } else {
            auto it_prev = std::prev(it);
            t_left = it_prev->first;
            r_left = it_prev->second;
            if (it == rates.end()) {
                t_right = t_left + 1;  // t_right = t_left => zero division
                r_right = r_left;      // constant yield beyond last data point
            } else {
                t_right = it->first;
                r_right = it->second;
            }
        }
        double r_eff = (r_left * (t_right - t) + r_right * (t - t_left)) /
                       (t_right - t_left);
        return std::exp(-r_eff * t / 360);
    }

    // Overwrites by default
    void add_rate(int tenor, double rate) {
        rates.insert_or_assign(tenor, rate);
    }

    // Utility: we can "template <class f> finally" if necessary ("f func")
    struct finally {
        std::function<void(void)> func;
        ~finally() { func(); }
    };

    // REQUIRES tenor to exist, RETURNS finally
    [[nodiscard]] finally bump_tenor(int tenor, double bump_amount) {
        Log::info_bump_tenor(tenor, bump_amount);
        rates.at(tenor) += bump_amount;
        return {[=, this]() {  // capturing local vars by reference can cause UB
            Log::info_unbump_tenor(tenor, bump_amount);
            rates.at(tenor) -= bump_amount;
        }};
    }

    // RETURNS finally
    [[nodiscard]] finally bump_curve(double bump_amount) {
        Log::info_bump_curve(bump_amount);
        for (auto const& tenor : get_tenors()) {
            Log::info_bump_tenor(tenor, bump_amount);
            rates.at(tenor) += bump_amount;
        }
        return {[=, this]() {  // capturing local vars by reference can cause UB
            Log::info_unbump_curve(bump_amount);
            for (auto const& tenor : get_tenors()) {
                Log::info_unbump_tenor(tenor, bump_amount);
                rates.at(tenor) -= bump_amount;
            }
        }};
    }
#ifdef DEBUG
    int x = 3;
    int* getX() { return &x; }
#endif
   private:
    std::map<int, double> rates;
    // Only used for typing (e.g. in get_tenors)
    static inline const decltype(rates)* r = nullptr;
};

/* Maintains and manipulates a FX spot rate */
struct FXSpot {
    // The user should calculate crosses assuming spots are stored as AAAUSD
    // That is indeed the case now so no inversion needed
    double operator/(const FXSpot& FX_term) const {
        return spot / FX_term.get_spot();
    }
    void set_spot(double s) { spot = s; }
    double get_spot() const { return spot; }

   private:
    double spot{1.0};  // defaults to 1 for USD
};

/* Maintains a list of maturity dates and the notionals on those dates*/
struct DateNotionals {
    // std::ranges::keys_view<std::views::all_t<decltype((date_notionals))>>
    auto get_maturities() const { return std::views::keys(date_notionals); }

    double get_book_value(const std::function<double(int)>& discount_factors) {
        std::vector<double> pvs(date_notionals.size());
        Log::info_date_notionals();
        // map-reduce
        std::transform(
            date_notionals.begin(), date_notionals.end(), pvs.begin(),
            [&discount_factors,
             delta = this->delta](std::pair<int, int> kv) -> double {
                auto& [date, notional] = kv;
                int eff_date{date - delta};
                double df{discount_factors(eff_date)};  // don't declare as int!
                Log::info_date_notionals_line(eff_date, notional, df);
                return notional * df;
            });
        double total = std::reduce(pvs.begin(), pvs.end());
        Log::info_book_value(total);
        return total;
    }

    void add_trade(int date, int notional) { date_notionals[date] += notional; }

    void set_delta(int d) { delta = d; }

   private:
    std::unordered_map<int, int> date_notionals;
    int delta{0};  // can roll, delete matured trades etc...
};
