# Gavin Faucet 5 BTC Analysis

This project analyzes Gavin Andresen’s Bitcoin faucet from 2010, specifically the 5 BTC payouts.

## Overview

Gavin’s faucet gave away 5 BTC per claim to early Bitcoin users. This project tracks:

- How many 5 BTC outputs (UTXOs) were created by the faucet
- How many of them remain unspent today

## Findings

- Total 5 BTC payouts created: 2,471  
- Remaining unspent 5 BTC UTXOs: 779  
- Total value of remaining UTXOs: 3,895 BTC (~31.53% of all 5 BTC payouts)  

> Note: Only 5 BTC payouts are counted. Later smaller payouts (0.5 BTC and less) are not included.

Many of the remaining UTXOs are likely permanently lost, as early users often did not back up their wallets or forgot about them.

## Files

- `all_5BTC_UTXOs.txt` – list of all 5 BTC outputs created by the faucet  
- `remaining_5BTC_UTXOs.txt` – list of 5 BTC outputs that remain unspent  

## Code

The analysis scripts are included for transparency and reproducibility. Anyone can use them to verify or extend the analysis.

## License

This project is released under the MIT License. Feel free to use and modify the code and data.
