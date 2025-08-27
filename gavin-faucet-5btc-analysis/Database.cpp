#include <iostream>
#include <string>
#include <fmt/format.h>
#include <list>
#include <fstream>
#include <sqlite3.h>
#include <openssl/ssl.h>

#include "Database.h"

using std::cout;
using std::cerr;
using std::endl;
using std::list;
using std::ofstream;
using std::to_string;
using std::exception;

Database::~Database()
{
    sqlite3_close(db);
    errorFile.close();
}

Database::Database() {
    int ret = 0;
    // initialize engine
    if (SQLITE_OK != (ret = sqlite3_initialize()))
    {
        printf("Failed to initialize library: %d\n", ret);
        throw exception();
    }

    if ( sqlite3_libversion_number() < 3027000 ) {
        printf("Failed to initialize library, version too old: %i\n", sqlite3_libversion_number());
        throw exception();
    }

    errorFile.open("errorFile");
}

bool Database::create(const string& name) {
    int ret = 0;
    if (SQLITE_OK != (ret = sqlite3_open_v2(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)))
    {
        printf("Failed to open conn: %d\n", ret);
        throw exception();
    }

    string createQuery = "CREATE TABLE IF NOT EXISTS transactions \
        (id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, txid TEXT NOT NULL, \
        UNIQUE(txid) \
    );";

    sqlite3_stmt *createStmt;

    sqlite3_prepare(db, createQuery.c_str(), createQuery.size(), &createStmt, NULL);
    if (sqlite3_step(createStmt) != SQLITE_DONE) {
        cout << "Didn't Create Table transactions!" << endl;
        throw exception();
    }
    sqlite3_finalize(createStmt);

    createQuery = "CREATE TABLE IF NOT EXISTS utxo (\
        id INTEGER NOT NULL, \
        voutn INTEGER NOT NULL, \
        value INTEGER NOT NULL, \
        voutaddress TEXT NOT NULL, \
        FOREIGN KEY (id) REFERENCES transactions(id) \
        UNIQUE(id, voutn, value, voutaddress) \
    );";

    sqlite3_prepare(db, createQuery.c_str(), createQuery.size(), &createStmt, NULL);
    if (sqlite3_step(createStmt) != SQLITE_DONE) {
        cout << "Didn't Create Table utxo!" << endl;
        throw exception();
    }
    sqlite3_finalize(createStmt);

    return true;
}

void Database::beginTransaction() {
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
}

void Database::commit() {
    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
}

void Database::rollback() {
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
}

vector<Utxo> Database::insertUtxo(const string &txid, int n, int64_t value, const string &address) {
    sqlite3_stmt *stmt;

    string sql = "INSERT OR IGNORE INTO transactions (txid) VALUES ('{0}');";
    sql = fmt::format(sql, txid);
    if (sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert transaction): %s\n", sqlite3_errmsg(db));
        throw exception();
    }
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cout << "Didn't insert or ignore transaction! " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        throw exception();
    }
    sqlite3_finalize(stmt);

    sql = "SELECT id FROM transactions WHERE txid='{0}' LIMIT 1;";
    sql = fmt::format(sql, txid);
    if (sqlite3_prepare(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "SQL error (select transaction id): %s\n", sqlite3_errmsg(db));
        throw exception();
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        cout << "Couldn't fetch transaction id for txid=" << txid << endl;
        sqlite3_finalize(stmt);
        throw exception();
    }
    int64_t id = stoll(string((const char*)sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);

    sql = "INSERT INTO utxo (id, voutn, value, voutaddress) VALUES ({0}, {1}, {2}, '{3}');";
    sql = fmt::format(sql, id, n, value, address);
    sqlite3_stmt *insertStmt;
    if (sqlite3_prepare(db, sql.c_str(), sql.size(), &insertStmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert utxo): %s\n", sqlite3_errmsg(db));
        throw exception();
    }
    int rc = sqlite3_step(insertStmt);
    sqlite3_finalize(insertStmt);

    vector<Utxo> inserted;

    if (rc == SQLITE_DONE) {
        Utxo utxo;
        utxo.id = id;
        utxo.txid = txid;
        utxo.voutn = n;
        utxo.value = value;
        utxo.voutaddress = address;
        inserted.push_back(utxo);
    } else {
        errorFile << "Didn't Insert UTXO: " << id << " " << n << " " << value << " " << address << endl;
        errorFile << "Error: " << sqlite3_errcode(db) << " - " << sqlite3_errmsg(db) << endl;
    }

    return inserted;
}

vector<Utxo> Database::deleteUtxo(const string& txid, int n) {
    sqlite3_stmt *stmt;

    string select = "SELECT u.id, u.value, u.voutaddress "
                    "FROM utxo u "
                    "JOIN transactions t ON u.id = t.id "
                    "WHERE t.txid='{0}' AND u.voutn={1} LIMIT 1;";
    select = fmt::format(select, txid, n);
    int rc = sqlite3_prepare(db, select.c_str(), -1, &stmt, 0);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (select utxo): %s\n", sqlite3_errmsg(db));
        throw exception();
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) {
            return {};
        } else {
            throw exception();
        }
    }

    vector<Utxo> utxos;
    Utxo utxo;
    utxo.txid = txid;
    utxo.voutn = n;
    int64_t id = stoll(string((const char*)sqlite3_column_text(stmt, 0)));
    utxo.value = stoll(string((const char*)sqlite3_column_text(stmt, 1)));
    utxo.voutaddress = string((const char*)sqlite3_column_text(stmt, 2));
    utxo.id = id;
    utxos.push_back(utxo);
    sqlite3_finalize(stmt);

    string sql = "DELETE FROM utxo WHERE id={0} AND voutn={1};";
    sql = fmt::format(sql, id, n);
    rc = sqlite3_prepare(db, sql.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (delete utxo): %s\n", sqlite3_errmsg(db));
        throw exception();
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cout << "Didn't DELETE UTXO! rc=" << rc << endl;
        throw exception();
    }
    return utxos;
}

vector<Utxo> Database::getUtxo(string txid, int n) {
    sqlite3_stmt *stmt;
    //        int rc = sqlite3_prepare(db, "SELECT block, timestamp FROM actualBlock", -1, &stmt, 0);
//    string select = "SELECT txid, voutn, value, voutaddress FROM utxo WHERE txid='{0}' AND voutn={1}";


    string select = "select transactions.txid, utxo.voutn, utxo.value, utxo.voutaddress \
        from utxo \
        join transactions using (id) \
        where transactions.txid='{0}' \
        AND utxo.voutn={1};";


    select = fmt::format(select, txid, n);
    int rc = sqlite3_prepare(db, select.c_str(), -1, &stmt, 0);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        throw exception();
    }
        rc = sqlite3_step(stmt);
        int ncols = sqlite3_column_count(stmt);

        if ( ncols != 4 ) {
            cout << "ncols != 4" << endl;
            throw exception();
        }

    vector<Utxo> utxos;
    while ( sqlite3_column_text(stmt, 0) ) {
        Utxo utxo;
        utxo.txid = string((const char*)sqlite3_column_text(stmt, 0));
        utxo.voutn = stoi(string((const char*)sqlite3_column_text(stmt, 1)));
        utxo.value = stoll(string((const char*)sqlite3_column_text(stmt, 2)));
        utxo.voutaddress = string((const char*)sqlite3_column_text(stmt, 3));
        utxos.push_back(utxo);
        rc = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    return utxos;
}
