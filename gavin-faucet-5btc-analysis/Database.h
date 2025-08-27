#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include <fstream>

#include "Utxo.h"

using std::string;
using std::vector;
using std::ofstream;

class Database {

  public:

    Database();

    ~Database();


    bool create(const string& name);

    void beginTransaction();

    void commit();

    void rollback();

    vector<Utxo> insertUtxo(const string& txid, int n, int64_t value, const string& address);

    vector<Utxo> deleteUtxo(const string& txid, int n);

    vector<Utxo> getUtxo(string txid, int n);

  private:

    sqlite3* db = NULL;

    ofstream errorFile;
};
