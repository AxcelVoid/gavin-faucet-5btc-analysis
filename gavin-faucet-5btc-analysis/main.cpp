#include <iostream>
#include <map>

#include <bitcoinapi/bitcoinapi.h>
#include <fmt/format.h>

#include "Database.h"

using namespace std;

int64_t parseValue(long double val) {
    std::stringstream ss;
    ss.precision(8);
    ss << std::fixed << val;
    string v = ss.str();
    int64_t n1 = static_cast<int64_t>(val);
    int pointPos = v.find(".");
    if ( pointPos == string::npos ) {
        return n1 * 100000000;
    }
    string part1 = v.substr(0,pointPos);
    string part2 = v.substr(pointPos+1);
    while ( part2.size() != 8 ) {
        part2 += "0";
    }
    return strtoll(string(part1 + part2).c_str(), NULL, 10);
}

bool isUTXOSpent(const utxoinfo_t& utxo) {
    return utxo.bestblock.empty() || utxo.scriptPubKey.hex.empty();
}

int main() {
    int startingBlock = 60433;  // first transaction to 15VjRaDX9zpbA8LVnbrCAFzrVzN7ixHNsC (149a3cab1d8ddcae55f472eb0ad2d10cd17650bc1480e266ff18de4bb31a1525)
    int lastBlock = 72740;      // last 5BTC transaction from Gavin's faucet
    Database db;
    db.create("db.db");

    vector<Utxo> utxosWith5Btc;

    cout.precision(8);
    cout << std::fixed;
    try
    {
        BitcoinAPI btc("btc", "satoshi", "satoshiworkshop.com", "node.satoshiworkshop.com", 80, 500000);

        int block = startingBlock;

        string blockhash = btc.getblockhash(block);
        blockinfo_t blockInfo = btc.getblock(blockhash);

        for ( ; block <= lastBlock; ++block ) {
            cout.flush();

            for ( auto& tx : blockInfo.tx ) {
                getrawtransaction_t tra = btc.getrawtransaction(tx, 1);

                bool foundUtxo = false;
                for ( auto& vin : tra.vin ) {
                    auto v = db.getUtxo(vin.txid, vin.n);
                    if ( !foundUtxo && !v.empty() ) {
                        foundUtxo = true;
                    }
                }

                for ( auto& vout : tra.vout ) {
                    if (vout.scriptPubKey.type == "pubkey") {
                        continue;
                    } else {
                        int64_t val = parseValue(vout.value);
                        if ( vout.scriptPubKey.addresses.empty() ) {
                            cout << "(" << block << ") [" << tx << ":" << vout.n << "] addresses empty!" << endl;
                            db.insertUtxo(tx, vout.n, val, "UNKNOWN");
                        } else {
                            for ( auto& address : vout.scriptPubKey.addresses ) {
                                if ( address == "15VjRaDX9zpbA8LVnbrCAFzrVzN7ixHNsC" ) {
                                    string valStr = fmt::format("{:.8f}", val / 100000000.0);
                                    cout << "(" << block << ") [" << tx << ":" << vout.n << "] gavin's faucet receiving " << valStr << "BTC" << endl;
                                    auto utxo = db.insertUtxo(tx, vout.n, val, address);
                                } else {
                                    if ( foundUtxo ) {
                                        if ( val == 500000000 ) {
                                            cout << "(" << block << ") [" << tx << ":" << vout.n << "] gavin's faucet sending 5BTC to address: "
                                                 << address << endl;
                                            utxosWith5Btc.emplace_back(0, tx, vout.n, val, address);
                                        } else if ( val >= 500000000 ) {
                                            cout << "(" << block << ") [" << tx << ":" << vout.n << "] gavin's change address: " << address << endl;
                                            auto utxo = db.insertUtxo(tx, vout.n, val, address);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // delete any known utxo from vin
                for ( const auto& vin : tra.vin ) {
                    auto utxo = db.deleteUtxo(vin.txid, vin.n);
                }
            }

            if ( blockInfo.nextblockhash.empty() ) {
                break;
            }

            blockInfo = btc.getblock(blockInfo.nextblockhash);
        }

        db.commit();

        cout << endl;
        cout << "Scan completed..." << endl;
        cout << "Found " << utxosWith5Btc.size()
             << " 5 BTC UTXOs created by Gavin's faucet" << endl;
        vector<Utxo> remaining5Btc;
        cout << "Checking which of these UTXOs are still unspent..." << endl;
        for ( const auto& u : utxosWith5Btc ) {
            if ( !isUTXOSpent(btc.gettxout(u.txid, u.voutn, false)) ) {
                remaining5Btc.push_back(u);
            }
        }
        double percentRemaining = 100.0 * remaining5Btc.size() / utxosWith5Btc.size();
        cout << "Out of those, " << remaining5Btc.size()
             << " remain unspent (totaling " << remaining5Btc.size() * 5
             << " BTC, " << percentRemaining << "% of all 5 BTC UTXOs)" << endl;
        ofstream all("all_5BTC_UTXOs.txt");
        for ( const auto& u : utxosWith5Btc ) {
            all << u.voutaddress << " [" << u.txid << ":" << u.voutn << "]" << endl;
        }
        all.close();
        ofstream unspent("remaining_5BTC_UTXOs.txt");
        for ( const auto& u : remaining5Btc ) {
            unspent << u.voutaddress << " [" << u.txid << ":" << u.voutn << "]" << endl;
        }
        unspent.close();
        cout << "Generated 2 files: all_5BTC_UTXOs.txt and remaining_5BTC_UTXOs.txt" << endl;
    } catch(BitcoinException& e) {
        cout << e.getMessage() << std::endl;
        db.rollback();
    } catch( std::exception& e ) {
        cout << e.what() << std::endl;
        db.rollback();
    }
    return 0;
}
