#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#ifdef DEBUG
#define err_stream std::cerr
#define out_stream std::cout
#else
#define err_stream null_stream
#define out_stream null_stream
#endif

struct Log {
    static void warn_data(const std::string& path) {
        make_red(err_stream);
        err_stream << "Could not read file at " << path << "\n";
    }
    static void warn_line(const std::string& line) {
        make_red(err_stream);
        err_stream << "Unrecognized line: " << line << "\n";
    }
    static void warn_ccy_str(const std::string& ccy_string) {
        make_red(err_stream);
        err_stream << "Unrecognized currency str: " << ccy_string << "\n";
    }
    static void warn_rates(const std::string& ccy_string) {
        make_red(err_stream);
        err_stream << "No rates for " << ccy_string << "\n";
    }
    static void warn_tenor_val(int tenor) {
        make_red(err_stream);
        err_stream << "Rate tenor " << tenor << " cannot be negative.\n";
    }
    static void warn_tenor_char(char name) {
        make_red(err_stream);
        err_stream << "Unrecognized tenor char: " << name << "\n";
    }
    static void warn_tenor_rate(const std::string& ccy_string, int tenor) {
        make_red(err_stream);
        err_stream << "Currency " << ccy_string << " has no tenor " << tenor
                   << "\n";
    }
    static void warn_fx(const std::string& ccy_string) {
        make_red(err_stream);
        err_stream << "No spot for " << ccy_string << "\n";
    }

    static void info_rate(const std::string& line) {
        make_yellow(out_stream);
        out_stream << "Parsing rate data: " << line << "\n";
    }
    static void info_fx(const std::string& line) {
        make_yellow(out_stream);
        out_stream << "Parsing FX data: " << line << "\n";
    }
    static void info_trade(const std::string& line) {
        make_yellow(out_stream);
        out_stream << "Parsing trade data: " << line << "\n";
    }

    static void info_delta(int delta) {
        make_green(out_stream);
        out_stream << "Actual delta (which we override for testing) is "
                   << delta << "\n";
    }
    static void info_effective_tenor_notional(int tenor, int notional) {
        make_green(out_stream);
        out_stream << "\tEffective tenor is " << tenor << " days, notional is "
                   << notional << "\n";
    }
    static void info_maturities(const std::string& ccy_string) {
        make_green(out_stream);
        out_stream << "Fetching all trade maturities for " << ccy_string
                   << "\n";
    }
    static void info_tenors(const std::string& ccy_string) {
        make_green(out_stream);
        out_stream << "Fetching available rate tenors for " << ccy_string
                   << "\n";
    }
    static void info_discounts(const std::string& ccy_string, int tenor) {
        make_green(out_stream);
        out_stream << "Calculating discount factor for " << ccy_string
                   << ", tenor = " << tenor << " days\n";
    }
    static void info_fx(const std::string& base, const std::string& term) {
        make_green(out_stream);
        out_stream << "Calculating FX spot for " << base << term << "\n";
    }
    static void info_DV01(const std::string& ccy_string, int tenor) {
        make_green(out_stream);
        out_stream << "Calculating DV01 with central differences for "
                   << ccy_string << " and a bump to tenor = " << tenor << "\n";
    }
    static void info_DV01(const std::string& ccy_string) {
        make_green(out_stream);
        out_stream << "Calculating DV01 with central differences for "
                   << ccy_string << " and a parallel curve shift\n";
    }
    static void info_bump_tenor(int tenor, double bump_amount) {
        make_green(out_stream);
        out_stream << "Bumping " << tenor << " days tenor by " << bump_amount
                   << "\n";
    };
    static void info_unbump_tenor(int tenor, double bump_amount) {
        make_green(out_stream);
        out_stream << "Unbumping " << tenor << " days tenor by " << bump_amount
                   << "\n";
    };
    static void info_bump_curve(double bump_amount) {
        make_green(out_stream);
        out_stream << "Bumping whole curve by " << bump_amount << "\n";
    }
    static void info_unbump_curve(double bump_amount) {
        make_green(out_stream);
        out_stream << "Unbumping whole curve by " << bump_amount << "\n";
    }
    static void info_date_notionals() {
        make_green(out_stream);
        out_stream << "Tenors\tNotional\tDiscount Factor\n";
    }
    static void info_date_notionals_line(int eff_date, double notional,
                                         double df) {
        make_green(out_stream);
        out_stream << "" << eff_date << "\t" << notional << "\t" << df << "\n";
    }
    static void info_book_value(double amount) {
        make_green(out_stream);
        out_stream << "Book PV of positions in chosen currency is " << amount
                   << "\n";
    }

    // Always print test output
    static void print_test_name(const std::string name) {
        make_blue(std::cout);
        std::cout << name << "\n";
    }
    static void print_test_double(double value) {
        make_blue(std::cout);
        std::cout << value << "\n";
    }
    template <typename T>
        requires std::integral<T> || std::floating_point<T>
    static void print_test_vector(std::vector<T> vec) {
        make_blue(std::cout);
        for (T& elem : vec) {
            std::cout << elem << " ";
        }
        std::cout << "\n";
    }

   private:
    static inline std::ofstream null_stream{"/dev/null"};
    static void make_red(std::ostream& os) {
        os << "\033[31m[WARN] \033[0m";  // set red output and reset
    }
    static void make_green(std::ostream& os) {
        os << "\033[32m[INFO] \033[0m";  // set green output and reset
    }
    static void make_blue(std::ostream& os) {
        os << "\033[34m[TEST] \033[0m";  // set blue output and reset
    }
    static void make_yellow(std::ostream& os) {
        os << "\033[33m[DATA] \033[0m";  // set yellow output and reset
    }
};
