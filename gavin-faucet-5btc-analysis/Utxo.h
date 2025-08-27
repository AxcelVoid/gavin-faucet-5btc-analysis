#pragma once

#include <string>
#include <utility>

using std::string;

struct Utxo {

	Utxo() {}
	Utxo(int id, string txid, int voutn, int64_t value, string voutaddress)
    : id(id)
    , txid(std::move(txid))
    , voutn(voutn)
    , value(value)
    , voutaddress(std::move(voutaddress))
    {}

	bool operator==(const Utxo& utxo) const {
        return 
			utxo.id == id &&
			utxo.txid == txid &&
			utxo.voutn == voutn &&
			utxo.value == value && 
			utxo.voutaddress == voutaddress;
	}

    int id;
    string txid;
    int voutn;
    int64_t value;
    string voutaddress;
};
