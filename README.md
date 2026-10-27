# Mini Backtester in C++: skill or luck?

A small C++ program that tests three simple trading rules on daily stock
prices and compares them with buy-and-hold, including trading fees.
It also has a "luck test" that checks whether a rule's result could
have happened by chance.

## Rules tested
- Trend: hold when the 5-day average price is above the 20-day average
- Momentum: hold when today's price is above the price 10 days ago
- Buy the dip: buy when price is 3% below its 20-day average, exit when
  it recovers to the average, or stop-loss at 10% below

## Luck test
For each rule, I keep its exact buy/sell pattern but slide it to start
on a different day, over and over. This keeps the number of trades the
same but makes the timing random. Luck% = share of random timings that
did as well as the real rule. Low is good, high means it may be luck.

## Data
Daily closing prices of [stock name], [start date] to [end date]
(302 days), from Yahoo Finance.

## Results (0.1% fee per buy/sell)
| Rule | 1 rupee became | Max drop | Trades | Win% | Luck% |
|---|---|---|---|---|---|
| Trend (5/20) | 0.905 | 27.3% | 10 | 40.0 | 84.4 |
| Momentum (10d) | 0.915 | 26.0% | 20 | 50.0 | 75.4 |
| Buy the dip | 0.913 | 21.4% | 8 | 50.0 | 80.7 |
| Buy and hold | 1.170 | 33.4% | 1 | - | - |

## What I learned
All three rules did worse than buy-and-hold on this data, and 75-84% of
random timings did as well, so there is no evidence the rules had skill.
Buy-and-hold had the biggest drop, so the rules did give a smoother ride.

## Limits
- Only one stock and about one year of data, so results are noisy.
- Rules were not tuned or tested on separate data.
- No slippage (prices moving while you trade).
- Buy-and-hold starts on day 1, the rules start after ~20 warm-up days.
- The luck test is a simple sanity check, not a rigorous statistical test.

## How to run
- g++ -O2 -std=c++17 mini_backtester.cpp -o backtest
- ./backtest
- prices.txt: one closing price per line, oldest first.)
- Note: built with help from AI ; I ran, tested and modified it myself.


