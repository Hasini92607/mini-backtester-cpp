// ============================================================
//  MINI BACKTESTER  -  "Is my trading rule skill, or just luck?"
// ============================================================
//  What it does:
//    1. Reads daily stock prices from prices.txt (one price per line, oldest first)
//    2. Tests 3 simple trading rules + plain "buy and hold"
//    3. Shows money made, biggest drop, number of trades, win rate
//    4. LUCK TEST: replays each rule's buy/sell pattern at every
//       possible random starting point. If many random timings do
//       just as well, the rule's good result was probably luck.
//
//  Build:  g++ -O2 -std=c++17 mini_backtester.cpp -o backtest
//          (optional, faster luck test: add -fopenmp)
//  Run:    ./backtest
// ============================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
using namespace std;

const double COST = 0.001;  // 0.1% fee every time we buy or sell

// ------------------------------------------------------------
//  PART 1: Reading prices from a file
// ------------------------------------------------------------
vector<double> loadPrices(const string& filename) {
    ifstream file(filename);
    vector<double> price;
    double p;
    while (file >> p) {
        price.push_back(p);
    }
    return price;
}

// ------------------------------------------------------------
//  PART 2: A helper - average of the last "days" prices ending at day "end"
// ------------------------------------------------------------
double average(const vector<double>& price, int end, int days) {
    double sum = 0;
    for (int k = end - days + 1; k <= end; k++) {
        sum += price[k];
    }
    return sum / days;
}

// ------------------------------------------------------------
//  PART 3: The trading rules
//  Each rule returns a list called "signal", one number per day:
//      1 = "I hold the stock after today's close"
//      0 = "I am in cash"
//  A rule on day i may only look at prices up to day i (no peeking at the future!)
// ------------------------------------------------------------

// Rule 1 - TREND: hold when the 5-day average is above the 20-day average
vector<int> trendRule(const vector<double>& price) {
    int n = price.size();
    vector<int> signal(n, 0);
    for (int i = 19; i < n; i++) {
        if (average(price, i, 5) > average(price, i, 20)) {
            signal[i] = 1;
        }
    }
    return signal;
}

// Rule 2 - MOMENTUM: hold when today's price is higher than 10 days ago
vector<int> momentumRule(const vector<double>& price) {
    int n = price.size();
    vector<int> signal(n, 0);
    for (int i = 10; i < n; i++) {
        if (price[i] > price[i - 10]) {
            signal[i] = 1;
        }
    }
    return signal;
}

// Rule 3 - BUY THE DIP: buy when price is 3% below its 20-day average,
//          then exit when it recovers to the average (take profit)
//          or falls to 10% below the average (stop-loss).
vector<int> dipRule(const vector<double>& price) {
    int n = price.size();
    vector<int> signal(n, 0);
    bool holding = false;
    for (int i = 19; i < n; i++) {
        double avg = average(price, i, 20);
        if (!holding && price[i] < avg * 0.97) {
            holding = true;                       // entry
        } else if (holding && price[i] >= avg) {
            holding = false;                      // exit: profit target
        } else if (holding && price[i] < avg * 0.90) {
            holding = false;                      // exit: stop-loss
        }
        signal[i] = holding ? 1 : 0;
    }
    return signal;
}

// Baseline - BUY AND HOLD: buy on day 1 and never sell
vector<int> buyAndHold(const vector<double>& price) {
    return vector<int>(price.size(), 1);
}

// ------------------------------------------------------------
//  PART 4: The engine - replays history using a signal and keeps score
// ------------------------------------------------------------
struct Result {
    double finalMoney = 1.0;   // 1 rupee grows to this
    double maxDrop = 0.0;      // biggest fall from a peak (0.2 = 20%)
    int trades = 0;            // number of times we bought
    double winRate = 0.0;      // % of trades that made money
    vector<double> equity;     // money on each day (for charts)
};

Result runBacktest(const vector<double>& price, const vector<int>& signal,
                   bool saveCurve) {
    int n = price.size();
    Result r;
    double money = 1.0;
    double peak = 1.0;
    double moneyAtBuy = 1.0;
    bool holding = false;
    int wins = 0, closedTrades = 0;

    if (saveCurve) r.equity.push_back(money);

    for (int i = 0; i < n - 1; i++) {
        bool wantToHold = (signal[i] == 1);

        // Buy
        if (wantToHold && !holding) {
            money *= (1 - COST);
            moneyAtBuy = money;
            holding = true;
            r.trades++;
        }
        // Sell
        else if (!wantToHold && holding) {
            money *= (1 - COST);
            if (money > moneyAtBuy) wins++;
            closedTrades++;
            holding = false;
        }

        // Tomorrow's price move only helps (or hurts) us if we hold
        if (holding) {
            money *= price[i + 1] / price[i];
        }

        // Track the biggest drop from the highest point so far
        peak = max(peak, money);
        double drop = (peak - money) / peak;
        r.maxDrop = max(r.maxDrop, drop);

        if (saveCurve) r.equity.push_back(money);
    }

    // If still holding at the very end, count that trade too
    if (holding) {
        if (money > moneyAtBuy) wins++;
        closedTrades++;
    }

    r.finalMoney = money;
    r.winRate = (closedTrades > 0) ? 100.0 * wins / closedTrades : 0.0;
    return r;
}

// ------------------------------------------------------------
//  PART 5: The LUCK TEST (the part that makes this project mine)
//
//  Idea: keep the rule's exact buy/sell pattern, but slide it to start
//  on a different day (wrapping around the end). The number of trades and
//  days in the market stay the same, only the TIMING becomes random.
//  We then count how many random timings did as well as the real rule.
//
//  Result is a percentage:
//     small (like 2%)  -> hard to get by luck, rule probably has something
//     large (like 40%) -> easy to get by luck, don't trust the rule
// ------------------------------------------------------------
double luckPercent(const vector<double>& price, const vector<int>& signal,
                   double realMoney) {
    int n = signal.size();
    int asGoodCount = 0;

    // (If you compile with -fopenmp, this loop uses all your CPU cores.
    //  Without it, the next line is simply ignored and the code still works.)
    #pragma omp parallel for reduction(+ : asGoodCount)
    for (int shift = 1; shift < n; shift++) {
        vector<int> shifted(n);
        for (int i = 0; i < n; i++) {
            shifted[(i + shift) % n] = signal[i];
        }
        Result r = runBacktest(price, shifted, false);
        if (r.finalMoney >= realMoney) {
            asGoodCount++;
        }
    }
    return 100.0 * asGoodCount / (n - 1);
}

// ------------------------------------------------------------
//  PART 6: main - run everything and print a table
// ------------------------------------------------------------
int main() {
    vector<double> price = loadPrices("prices.txt");
    int n = price.size();
    if (n < 60) {
        cout << "Please put at least 60 prices in prices.txt (one per line).\n";
        return 0;
    }
    cout << "Loaded " << n << " daily prices.\n\n";

    // The list of rules we want to test
    vector<string> names = {"Trend (5/20)", "Momentum (10d)", "Buy the dip",
                            "Buy and hold"};
    vector<vector<int>> signals = {trendRule(price), momentumRule(price),
                                   dipRule(price), buyAndHold(price)};

    // Table header
    cout << left << setw(16) << "Rule" << right << setw(10) << "Money"
         << setw(12) << "Max drop%" << setw(9) << "Trades" << setw(10)
         << "Win%" << setw(12) << "Luck%" << "\n";
    cout << string(69, '-') << "\n";

    // Run every rule
    vector<Result> results;
    for (size_t k = 0; k < names.size(); k++) {
        Result r = runBacktest(price, signals[k], true);
        results.push_back(r);

        cout << left << setw(16) << names[k] << right << fixed
             << setprecision(3) << setw(10) << r.finalMoney
             << setprecision(1) << setw(12) << r.maxDrop * 100 << setw(9)
             << r.trades << setw(10) << r.winRate;

        // Luck test makes no sense for buy-and-hold (it never changes)
        if (names[k] == "Buy and hold") {
            cout << setw(12) << "-";
        } else {
            double luck = luckPercent(price, signals[k], r.finalMoney);
            cout << setw(12) << luck;
        }
        cout << "\n";
    }

    cout << "\nHow to read this:\n";
    cout << " Money    : 1 rupee became this much after fees.\n";
    cout << " Max drop : worst fall from a peak. Smaller = smoother ride.\n";
    cout << " Win%     : share of trades that made money.\n";
    cout << " Luck%    : share of random timings that did as well.\n";
    cout << "            Under 5% is encouraging. Over 20% means it could be luck.\n";

    // Save money-over-time of every rule, so you can draw a chart in Excel
    ofstream out("equity.csv");
    out << "day";
    for (auto& name : names) out << "," << name;
    out << "\n";
    for (int day = 0; day < n; day++) {
        out << day;
        for (auto& r : results) out << "," << r.equity[day];
        out << "\n";
    }
    cout << "\nMoney-over-time saved to equity.csv (open in Excel to plot).\n";
    return 0;
}
