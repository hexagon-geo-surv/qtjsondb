/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtAddOn.JsonDb module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>
#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTime>


#include "hbtree.h"
#include "hbtreetransaction.h"
#include "hbtreecursor.h"
#include "hbtree_p.h"
#include "orderedlist_p.h"

class TestHBtree: public QObject
{
    Q_OBJECT
public:
    TestHBtree();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void orderedList();

    void openClose();
    void reopen();
    void reopenMultiple();
    void create();
    void addSomeSingleNodes();
    void splitLeafOnce_1K();
    void splitManyLeafs_1K();
    void testOverflow_3K();
    void testMultiOverflow_20K();
    void addDeleteNodes_100Bytes();
    void last();
    void first();
    void insertRandom_200BytesTo1kValues();
    void insertHugeData_10Mb();
    void splitBranch_1kData();
    void splitBranchWithOverflows();
    void cursorExactMatch();
    void cursorFuzzyMatch();
    void cursorNext();
    void cursorPrev();
    void lastMultiPage();
    void firstMultiPage();
    void prev();
    void prev2();
    void multiBranchSplits();
    void rollback();
    void multipleRollbacks();
    void createWithCmp();
    void readAndWrite();
    void variableSizeKeysAndData();
    void transactionTag();
    void compareSequenceOfVarLengthKeys();
    void asciiAsSortedNumbers();
    void deleteReinsertVerify_data();
    void deleteReinsertVerify();
    void rebalanceEmptyTree();
    void reinsertion();
    void nodeComparisons();
    void tag();
    void cursors();
    void markerOnReopen_data();
    void markerOnReopen();
    void corruptSyncMarker1_data();
    void corruptSyncMarker1();
    void corruptBothSyncMarkers_data();
    void corruptBothSyncMarkers();
    void corruptPingMarker_data();
    void corruptPingMarker();
    void corruptPongMarker_data();
    void corruptPongMarker();
    void corruptPingAndPongMarker_data();
    void corruptPingAndPongMarker();
    void epicCorruptionTest_data();
    void epicCorruptionTest();

private:
    void corruptSinglePage(int psize, int pgno = -1, qint32 type = -1);
    HBtree *db;
    HBtreePrivate *d;

    bool printOutCollectibles_;
};

const char * sizeStr(size_t sz)
{
    static char buffer[256];
    const size_t kb = 1024;
    const size_t mb = kb * kb;
    if (sz > mb) {
        sprintf(buffer, "%.2f mb", (float)sz / mb);
    } else if (sz > kb) {
        sprintf(buffer, "%.2f kb", (float)sz / kb);
    } else {
        sprintf(buffer, "%zu bytes", sz);
    }
    return buffer;
}

int myRand(int min, int max)
{
    float multiplier = (float)qrand() / (float)RAND_MAX;
    return (int)(multiplier * (float)(max - min)) + min;
}

int myRand(int r)
{
    return (int)(((float)qrand() / (float)RAND_MAX) * (float)r);
}

TestHBtree::TestHBtree()
    : db(NULL), printOutCollectibles_(true)
{
}

static const char dbname[] = "tst_HBtree.db";

void TestHBtree::initTestCase()
{
}

void TestHBtree::cleanupTestCase()
{
}

void TestHBtree::init()
{
    QFile::remove(dbname);
    db = new HBtree(dbname);
    db->setAutoSyncRate(100);
    if (!db->open(HBtree::ReadWrite))
        Q_ASSERT(false);
    d = db->d_func();
    printOutCollectibles_ = true;
}

void TestHBtree::cleanup()
{
    qDebug() << "Size:" << sizeStr(db->size()) << " Pages:" << db->size() / d->spec_.pageSize;
    qDebug() << "Stats:" << db->stats();
    if (printOutCollectibles_)
        qDebug() << "Collectible pages: " << d->collectiblePages_;

    delete db;
    db = 0;
    QFile::remove(dbname);
}

void TestHBtree::openClose()
{
    // init/cleanup does open and close;
    return;
}

void TestHBtree::reopen()
{
    db->close();
    QVERIFY(db->open());
    QCOMPARE(db->size(), (size_t)db->pageSize() * 5);
}

void TestHBtree::reopenMultiple()
{
    const int numItems = 32;
    const int keySize = 64;
    const int valueSize = 64;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    const int retries = 5;

    for (int i = 0; i < retries; ++i) {
        db->close();
        db->open();
        QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
        while (it != keyValues.end()) {
            HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(transaction);
            QByteArray result = transaction->get(it.key());
            QCOMPARE(result, it.value());
            transaction->abort();
            ++it;
        }
    }
}

void TestHBtree::create()
{
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");
    QByteArray key3("3");
    QByteArray value3("baz");

    QByteArray result;

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    // read non-existing entry
    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    QVERIFY(txn->commit(42));
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    txn->abort();

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    QVERIFY(txn->put(key3, value3));

    // read all
    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    QVERIFY(txn->commit(32));
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value2, result);

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->abort();

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->isReadWrite());

    QVERIFY(txn->remove(key2));

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->commit(22);
    QVERIFY(db->sync());

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->isReadOnly());

    result = txn->get(key1);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value1, result);

    result = txn->get(key2);
    QVERIFY(result.isEmpty());

    result = txn->get(key3);
    QVERIFY(!result.isEmpty());
    QCOMPARE(value3, result);

    txn->abort();
}

void TestHBtree::addSomeSingleNodes()
{
    const int numItems = 50;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QString::number(i).toAscii()));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QString::number(i).toAscii());
        transaction->abort();
    }
}

void TestHBtree::splitLeafOnce_1K()
{
    const int numBytes = 1000;
    const int numItems = 4;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::splitManyLeafs_1K()
{
    const int numBytes = 1000;
    const int numItems = 255;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::testOverflow_3K()
{
    const int numBytes = 3000;
    const int numItems = 10;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::testMultiOverflow_20K()
{
    const int numBytes = 20000; // must cause multiple overflow pages to be created
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::addDeleteNodes_100Bytes()
{
    const int numBytes = 200;
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
        QCOMPARE(i + 1, db->stats().numEntries);
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->remove(QString::number(i).toAscii()));
        transaction->commit(i);
        QCOMPARE(numItems - i - 1, db->stats().numEntries);
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray());
        transaction->abort();
    }
}
void TestHBtree::last()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write first entry
    QVERIFY(transaction->put(key1, value1));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(key1, outkey1);
    }

    // write second entry
    QVERIFY(transaction->put(key2, value2));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey2;
        cursor.current(&outkey2, 0);
        QCOMPARE(key2, outkey2);
    }

    // write zeroth entry
    QVERIFY(transaction->put(key0, value0));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.last());
        QByteArray outkey3;
        cursor.current(&outkey3, 0);
        QCOMPARE(key2, outkey3);
    }

    transaction->commit(42);
}

void TestHBtree::first()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write first entry
    QVERIFY(transaction->put(key1, value1));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(key1, outkey1);
    }

    // write second entry
    QVERIFY(transaction->put(key2, value2));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey2;
        cursor.current(&outkey2, 0);
        QCOMPARE(key1, outkey2);
    }

    // write zeroth entry
    QVERIFY(transaction->put(key0, value0));

    // test cursor->last()
    {
        HBtreeCursor cursor(transaction);
        QVERIFY(cursor.first());
        QByteArray outkey3;
        cursor.current(&outkey3, 0);
        QCOMPARE(key0, outkey3);
    }

    transaction->commit(42);
}

void TestHBtree::insertRandom_200BytesTo1kValues()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        QByteArray value(myRand(200, 1000) , 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::insertHugeData_10Mb()
{
    QSKIP("This test takes too long");

    const int numBytes = 10000000;
    const int numItems = 100;
    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(QString::number(i).toAscii(), QByteArray(numBytes, '0' + i)));
        QVERIFY(transaction->commit(i));
    }

    for (int i = 0; i < numItems; ++i) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QCOMPARE(transaction->get(QString::number(i).toAscii()), QByteArray(numBytes, '0' + i));
        transaction->abort();
    }
}

void TestHBtree::splitBranch_1kData()
{
    const int numItems = 5000; // must cause multiple branch splits
    const int valueSize = 1000;
    const int keySize = 16;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::splitBranchWithOverflows()
{
    // Bug, overflow pages get lost at a split.
    // This splits the page at the 308th insertion. The first
    // get transaction on a read fails after a split

    const int numItems = 1000; // must cause a split
    const int keySize = 255; // Only 3 1k keys can fit then we get a split
    const int valueSize = 4000; // must be over overflow threashold
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }

}

void TestHBtree::cursorExactMatch()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(10,1000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();

    while (it != keyValues.end()) {
        QVERIFY(cursor.seek(it.key()));
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }

    transaction->abort();
}

void TestHBtree::cursorFuzzyMatch()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(i * 2 + (i % 2)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    for (int i = 0; i < numItems * 2; ++i) {
        QByteArray ba = QString::number(i).toAscii();
        bool ok = cursor.seekRange(ba);
        QMap<QByteArray, QByteArray>::iterator it = keyValues.lowerBound(ba);

        if (it == keyValues.end())
            QVERIFY(!ok);
        else
            QCOMPARE(cursor.key(), it.key());
    }

    transaction->abort();
}

void TestHBtree::cursorNext()
{
    const int numItems = 500;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(0,100000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();

    while (it != keyValues.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }

    transaction->abort();
}

void TestHBtree::cursorPrev()
{
    const int numItems = 500;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray ba = QString::number(myRand(0,100000)).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(ba, ba));
        keyValues.insert(ba, ba);
        QVERIFY(transaction->commit(i));
    }

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    QMap<QByteArray, QByteArray>::iterator it = keyValues.end();

    if (keyValues.size()) {
        do {
                --it;
                QVERIFY(cursor.previous());
                QCOMPARE(cursor.key(), it.key());
        } while (it != keyValues.begin());
    }

    transaction->abort();
}

void TestHBtree::lastMultiPage()
{
    QByteArray value0("baz");

    for (int i = 0; i < 1024; i++) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(baKey, outkey1);
        while (cursor.previous()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) > 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestHBtree::firstMultiPage()
{
    QByteArray value0("baz");

    for (int i = 1024; i > 0; i--) {
        // write first entry
        QByteArray baKey(4, 0);
        qToBigEndian(i, (uchar *)baKey.data());
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(baKey, value0));
        QVERIFY(txn->commit(0));

        txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.first());
        QByteArray outkey1;
        cursor.current(&outkey1, 0);
        QCOMPARE(baKey, outkey1);
        while (cursor.next()) {
            QByteArray outkey2;
            cursor.current(&outkey2, 0);
            QVERIFY(memcmp(outkey1.constData(), outkey2.constData(), outkey1.size()) < 0);
            outkey1 = outkey2;
        }
        txn->abort();
    }
}

void TestHBtree::prev()
{
    QByteArray key0("0");
    QByteArray value0("baz");
    QByteArray key1("1");
    QByteArray value1("foo");
    QByteArray key2("2");
    QByteArray value2("bar");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // write entries
    QVERIFY(txn->put(key0, value0));
    QVERIFY(txn->put(key1, value1));
    QVERIFY(txn->put(key2, value2));

    // go to end
    {
        HBtreeCursor cursor(txn);
        QVERIFY(cursor.last());
        // test prev
        QVERIFY(cursor.previous());
        QByteArray outkey;
        cursor.current(&outkey, 0);
        QCOMPARE(key1, outkey);
    }

    {
        HBtreeCursor cursor(txn);
        // test prev without initialization is same as last()
        QVERIFY(cursor.previous());
        QByteArray outkey;
        cursor.current(&outkey, 0);
        QCOMPARE(key2, outkey);

        // prev to key1
        QVERIFY(cursor.previous());
        cursor.current(&outkey, 0);
        QCOMPARE(key1, outkey);

        // prev to key0
        QVERIFY(cursor.previous());
        cursor.current(&outkey, 0);
        QCOMPARE(key0, outkey);

        // prev to eof
        QVERIFY(!cursor.previous());
    }
    txn->abort();
}

void TestHBtree::prev2()
{
    QFile file(dbname);
    int maxSize = file.size();

    int amount = ::getenv("BENCHMARK_AMOUNT") ? ::atoi(::getenv("BENCHMARK_AMOUNT")) : 40000;
    for (int i = 0; i < amount; ++i) {
        QByteArray data = QUuid::createUuid().toRfc4122();
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
//        qDebug() << i;
        QVERIFY(txn->put(data, QByteArray("value_")+QByteArray::number(i)));
        txn->commit(0);
        int size = file.size();
        if (size > maxSize)
            maxSize = size;
    }

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor c(txn);
    QVERIFY(c.first());
    int cnt = 1;
    while (c.next()) ++cnt;
    QCOMPARE(cnt, amount);

    HBtreeCursor r(txn);
    QVERIFY(r.last());
    int rcnt = 1;
    while (r.previous()) ++rcnt;

    QCOMPARE(rcnt, amount);
    txn->abort();
}

void TestHBtree::multiBranchSplits()
{
    const int numItems = 40; // must cause multiple branch splits
    const int valueSize = 1;
    const int keySize = 1500;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        if (key.size() < keySize)
            key += QByteArray(keySize - key.size(), '-');
        QByteArray value(valueSize, 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    HBtreeCursor cursor(transaction);
    while (it != keyValues.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), it.key());
        ++it;
    }
    transaction->abort();
}

int keyCmp(const QByteArray &a, const QByteArray &b)
{
    QString as((const QChar*)a.constData(), a.size() / 2);
    QString bs((const QChar*)b.constData(), b.size() / 2);
    if (as < bs)
        return -1;
    else if (as > bs)
        return 1;
    else
        return 0;
}

void TestHBtree::createWithCmp()
{
    db->setCompareFunction(keyCmp);
    QString str1("1");
    QByteArray key1 = QByteArray::fromRawData((const char *)str1.data(), str1.size()*2);
    QByteArray value1("foo");
    QString str2("2");
    QByteArray key2 = QByteArray::fromRawData((const char *)str2.data(), str2.size()*2);
    QByteArray value2("bar");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);

    // write first entry
    QVERIFY(txn->put(key1, value1));

    // read it
    QCOMPARE(value1, txn->get(key1));

    // read non-existing entry
    QVERIFY(txn->get(key2).isEmpty());

    // write second entry
    QVERIFY(txn->put(key2, value2));

    // read both entries
    QCOMPARE(value1, txn->get(key1));
    QCOMPARE(value2, txn->get(key2));

    txn->abort();
}

void TestHBtree::rollback()
{
    QByteArray key1("22");
    QByteArray value1("foo");
    QByteArray key2("42");
    QByteArray value2("bar");

    QByteArray result;

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    // write first entry
    QVERIFY(txn->put(key1, value1));
    txn->commit(42);

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));

        QVERIFY(txn->put(key1, value2));

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        txn->abort();
    }

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);

    // read both entries
    QCOMPARE(value1, txn->get(key1));

    QVERIFY(txn->get(key2).isEmpty());

    txn->abort();
}

void TestHBtree::multipleRollbacks()
{
    QByteArray key1("101");
    QByteArray value1("foo");
    QByteArray key2("102");
    QByteArray value2("bar");

    {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        // write first entry
        QVERIFY(txn->put(key1, value1));
        QVERIFY(txn->commit(0));
    }

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // re-write the first entry
        QVERIFY(txn->remove(key1));
        QVERIFY(txn->put(key1, value2));

        // abort the transaction
        txn->abort();
    }

    {
        // start transaction
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);

        // write second entry
        QVERIFY(txn->put(key2, value2));

        // abort the transaction
        txn->abort();
    }

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);

    // read both entries
    QCOMPARE(value1, txn->get(key1));
    QVERIFY(txn->get(key2).isEmpty());
    txn->abort();
}

void TestHBtree::readAndWrite()
{
    HBtree &wdb = *db;

    HBtreeTransaction *wdbtxn = wdb.beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(wdbtxn);
    QVERIFY(wdbtxn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(wdbtxn->put(QByteArray("bla"), QByteArray("bla")));
    QVERIFY(wdbtxn->commit(1));

    HBtree rdb1;
    rdb1.setFileName(dbname);
    rdb1.setOpenMode(HBtree::ReadOnly);
    QVERIFY(rdb1.open());

    HBtreeTransaction *rdb1txn = rdb1.beginTransaction(HBtreeTransaction::ReadOnly);
    QByteArray value;
    QCOMPARE(rdb1txn->get("foo"), QByteArray("bar"));
    QCOMPARE(rdb1txn->get("bla"), QByteArray("bla"));
    rdb1txn->abort();

    wdbtxn = wdb.beginTransaction(HBtreeTransaction::ReadWrite);
    wdbtxn->put(QByteArray("foo2"), QByteArray("bar2"));
    wdbtxn->put(QByteArray("bar"), QByteArray("baz"));
    // do not commit yet

    rdb1txn = rdb1.beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rdb1txn);
    QVERIFY(rdb1txn->get("foo2").isEmpty());

    HBtree rdb2;
    rdb2.setFileName(dbname);
    rdb2.setOpenMode(HBtree::ReadOnly);
    QVERIFY(rdb2.open());

    HBtreeTransaction *rdb2txn = rdb2.beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rdb2txn);
    QVERIFY(!rdb2txn->get("foo").isEmpty());
    QVERIFY(rdb2txn->get("foo2").isEmpty());

    QVERIFY(wdbtxn->commit(2));

    QVERIFY(!rdb2txn->get("foo").isEmpty());
    QVERIFY(rdb2txn->get("foo2").isEmpty());

    rdb1txn->abort();
    rdb1.close();// should the beginTransaction below pick changes up automatically?
    rdb1.open();
    rdb1txn = rdb1.beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rdb1txn);
    QVERIFY(!rdb1txn->get("foo").isEmpty());
    QCOMPARE(rdb1txn->get("foo2"), QByteArray("bar2"));
    rdb1txn->abort();
    rdb2txn->abort();
}


void TestHBtree::variableSizeKeysAndData()
{
    QByteArray keyPrefix[10] = {
        QByteArray("0001234567890123456789"),
        QByteArray("000123456789"),
        QByteArray("00012345678"),
        QByteArray("0001234567"),
        QByteArray("000123456"),
        QByteArray("00012345"),
        QByteArray("0001234"),
        QByteArray("000123"),
        QByteArray("00012"),
        QByteArray("1")};

    /* initialize random seed: */
    srand ( 0 ); //QDateTime::currentMSecsSinceEpoch() );

    for (int i = 0; i < 1024; i++) {
        // Create a key with one of the prefixes from above
        // Start by selecting one of the key prefixes
        QByteArray key = keyPrefix[rand()%10];
        int length = rand() % 128 + 1;
        QByteArray keyPostfix(length, ' ');
        for (int j=0; j<length; j++) {
            keyPostfix[j] = quint8(rand()%255);
        }
        key += keyPostfix;

        length = rand() % 1024 + 1;
        // Create a random length value with random bytes
        QByteArray value(length, ' ');
        for (int j=0; j<length; j++) {
            value[j] = quint8(rand()%255);
        }
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(key, value));
        QVERIFY(txn->commit(0));
    }
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    // Delete every second object
    HBtreeCursor cursor(txn);
    QVERIFY(cursor.first());
    QByteArray key;
    cursor.current(&key, 0);
    bool remove = true;
    int counter = 0;
    while (cursor.next()) {
        counter++;
        if (remove) {
            remove = false;
            QVERIFY(txn->remove(cursor.key()));
        }
        else remove = true;
    }
    txn->commit(0);

    // Set this to false because after all those deleted we get a shit load of collectible pages
    // I don't know what to do with all of them. Commit them to disk on a new GC Page type?
    printOutCollectibles_ = false;
}

void TestHBtree::transactionTag()
{
    QSKIP("beginTransaction doesn't check for updated marker page. This won't work yet.");
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->put(QByteArray("bla"), QByteArray("bla")));
    QVERIFY(txn->commit(1));
    QCOMPARE(db->tag(), int(1));

    HBtree rdb;
    rdb.setFileName(dbname);
    rdb.setOpenMode(HBtree::ReadOnly);
    QVERIFY(rdb.open());
    QCOMPARE(rdb.tag(), (int)1);
    HBtreeTransaction *rdbtxn = rdb.beginTransaction(HBtreeTransaction::ReadOnly);
    QCOMPARE(rdb.tag(), (int)1);
    QCOMPARE(rdbtxn->tag(), (quint64)1);

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(2));
    QCOMPARE(db->tag(), (int)2);

    QCOMPARE(rdb.tag(), (int)1);
    rdbtxn->abort();

    rdbtxn = rdb.beginTransaction(HBtreeTransaction::ReadOnly);
    QCOMPARE(rdbtxn->tag(), (quint64)2);
    rdbtxn->abort();
}

int findLongestSequenceOf(const char *a, size_t size, char x)
{
    int result = 0;
    int count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (count > result)
            result = count;

        if (count) {
            if (a[i] == x)
                count++;
            else
                count = 0;
            continue;
        }

        count = a[i] == x ? 1 : 0;
    }

    if (count > result)
        result = count;

    return result;
}

int cmpVarLengthKeys(const QByteArray &a, const QByteArray &b)
{
    int acount = findLongestSequenceOf(a.constData(), a.size(), 'a');
    int bcount = findLongestSequenceOf(b.constData(), b.size(), 'a');

    if (acount == bcount) {
        return QString::compare(a, b);
    } else {
        return (acount > bcount) ? 1 : ((acount < bcount) ? -1 : 0);
    }
}

bool cmpVarLengthKeysForQVec(const QByteArray &a, const QByteArray &b)
{
    return cmpVarLengthKeys(a, b) < 0;
}


void TestHBtree::compareSequenceOfVarLengthKeys()
{
    const char sequenceChar = 'a';
    const int numElements = 1024;
    const int minKeyLength = 20;
    const int maxKeyLength = 25;

    db->setCompareFunction(cmpVarLengthKeys);

    // Create vector of variable length keys of sequenceChar
    QVector<QByteArray> vec;
    for (int i = 0; i < numElements; ++i) {
        QByteArray k(minKeyLength + myRand(maxKeyLength - minKeyLength), sequenceChar);

        // Change character at random indexed
        for (int j = 0; j < k.size(); ++j) {
            if (myRand(2) > 0)
                k[j] = 'a' + myRand(26);
        }
        vec.append(k);
    }

    for (int i = 0; i < vec.size(); ++i) {
        int count = findLongestSequenceOf(vec[i].constData(), vec[i].size(), sequenceChar);
        QByteArray value((const char*)&count, sizeof(count));
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(vec[i], vec[i]));
        QVERIFY(txn->commit(i));
    }

    // Sort QVector to use as verification of bdb sort order
    qSort(vec.begin(), vec.end(), cmpVarLengthKeysForQVec);

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor cursor(txn);

    QByteArray key;
    QByteArray value;
    int i = 0;
    while (cursor.next()) {
        cursor.current(&key, 0);
        cursor.current(0, &value);
        QCOMPARE(key, vec[i++]);
    }
    txn->abort();
}

int asciiCmpFunc(const QByteArray &a, const QByteArray &b) {
//    qDebug() << a << b;
    int na = a.toInt();
    int nb = b.toInt();
    return na < nb ? -1 : (na > nb ? 1 : 0);
}

bool asciiCmpFuncForVec(const QByteArray &a, const QByteArray &b) {
    return asciiCmpFunc(a, b) < 0;
}

void TestHBtree::asciiAsSortedNumbers()
{
    const int numItems = 1000;
    QVector<QByteArray> keys;

    db->setCompareFunction(asciiCmpFunc);

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, key));
        keys.append(key);
        QVERIFY(transaction->commit(i));
    }

    qSort(keys.begin(), keys.end(), asciiCmpFuncForVec);

    HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(transaction);
    HBtreeCursor cursor(transaction);
    QVector<QByteArray>::iterator it = keys.begin();
    while (it != keys.end()) {
        QVERIFY(cursor.next());
        QCOMPARE(cursor.key(), *it);
        QCOMPARE(cursor.value(),*it);
        ++it;
    }
    transaction->abort();
}

void TestHBtree::deleteReinsertVerify_data()
{
    QTest::addColumn<bool>("useCmp");
    QTest::newRow("With custom compare") << true;
    QTest::newRow("Without custom compare") << false;
}

void TestHBtree::deleteReinsertVerify()
{
    QFETCH(bool, useCmp);

    if (useCmp)
        db->setCompareFunction(asciiCmpFunc);

    const int numItems = 1000;

    QVector<QByteArray> keys;
    QMap<QByteArray, QByteArray> keyValueMap;
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(i).toAscii();
        QByteArray value = QString::number(rand()).toAscii();
        keys.append(key);
        keyValueMap.insert(key, value);
        QVERIFY(txn->put(key, value));
    }
    QVERIFY(txn->commit(100));

    if (useCmp)
        qSort(keys.begin(), keys.end(), asciiCmpFuncForVec);
    else
        qSort(keys); // sort by qbytearray oeprator < since that's default in HBtree

    QCOMPARE(keyValueMap.size(), db->count());

    // Remove every other key
    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QMap<QByteArray, QByteArray> removedKeyValues;
    for (int i = 0; i < numItems; i += 2) {
        int idx = i;
        QByteArray removedValue = keyValueMap[keys[idx]];
        QCOMPARE(keyValueMap.remove(keys[idx]), 1);
        removedKeyValues.insert(keys[idx], removedValue);
        QVERIFY(txn->remove(keys[idx]));
    }
    txn->commit(200);

    // Verify
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QMap<QByteArray, QByteArray>::iterator it = keyValueMap.begin();
    while (it != keyValueMap.end()) {
        QCOMPARE(txn->get(it.key()), it.value());
        ++it;
    }

    // Reinsert
    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    it = removedKeyValues.begin();
    while (it != removedKeyValues.end()) {
        QVERIFY(txn->put(it.key(), it.value()));
        keyValueMap.insert(it.key(), it.value());
        ++it;
    }
    txn->commit(300);

    // Verify in order.
    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    HBtreeCursor cursor(txn);
    for (int i = 0; i < numItems; ++i) {
        QVERIFY(cursor.next());
        it = keyValueMap.find(keys[i]);
        QVERIFY(it != keyValueMap.end());
        QCOMPARE(cursor.key(), keys[i]);
        QCOMPARE(cursor.value(), it.value());
    }
    txn->abort();

}

void TestHBtree::rebalanceEmptyTree()
{
    QByteArray k1("foo");
    QByteArray v1("bar");
    QByteArray k2("ding");
    QByteArray v2("dong");
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(k1, v1));
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QCOMPARE(txn->get(k1), v1);
    QVERIFY(txn->remove(k1));
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->put(k2, v2));
    QCOMPARE(txn->get(k2), v2);
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->remove(k2));
    QVERIFY(txn->commit(0));

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QVERIFY(txn->get(k1).isEmpty());
    QVERIFY(txn->get(k2).isEmpty());
    QVERIFY(txn->commit(0));
}

void TestHBtree::reinsertion()
{
    const int numItems = 1000;
    QMap<QByteArray, QByteArray> keyValues;

    for (int i = 0; i < numItems; ++i) {
        QByteArray key = QString::number(qrand()).toAscii();
        QByteArray value(myRand(200, 1000) , 'a' + i);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(key, value));
        keyValues.insert(key, value);
        QVERIFY(transaction->commit(i));
    }

    QMap<QByteArray, QByteArray>::iterator it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }

    for (int i = 0; i < numItems * 2; ++i) {
        it = keyValues.begin() + myRand(0, numItems);
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(transaction);
        QVERIFY(transaction->put(it.key(), it.value()));
        QVERIFY(transaction->commit(i));
    }

    it = keyValues.begin();
    while (it != keyValues.end()) {
        HBtreeTransaction *transaction = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(transaction);
        QByteArray result = transaction->get(it.key());
        QCOMPARE(result, it.value());
        transaction->abort();
        ++it;
    }
}

void TestHBtree::nodeComparisons()
{
    db->setCompareFunction(asciiCmpFunc);
    HBtreePrivate::NodeKey nkey10(d->compareFunction_, "10");
    HBtreePrivate::NodeKey nkey7(d->compareFunction_, "7");
    HBtreePrivate::NodeKey nkey33(d->compareFunction_, "33");

    QVERIFY(nkey10 > nkey7);
    QVERIFY(nkey33 > nkey7);
    QVERIFY(nkey33 > nkey10);

    QVERIFY(nkey10 != nkey7);
    QVERIFY(nkey33 != nkey7);
    QVERIFY(nkey33 != nkey10);

    QVERIFY(nkey7 < nkey10);
    QVERIFY(nkey7 < nkey33);
    QVERIFY(nkey10 < nkey33);

    QVERIFY(nkey10 == nkey10);

    QVERIFY(nkey7 <= nkey33);
    QVERIFY(nkey33 <= nkey33);

    QVERIFY(nkey33 >= nkey7);
    QVERIFY(nkey33 >= nkey33);
}

void TestHBtree::tag()
{
    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QVERIFY(txn->commit(42u));

    txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("123")));
    QCOMPARE(db->tag(), 42u);
    // do not commit just yet

    HBtreeTransaction *rtxn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 42u);

    QVERIFY(txn->commit(64u));
    QCOMPARE(db->tag(), 64u);
    QCOMPARE(rtxn->tag(), 42u);
    rtxn->abort();
    rtxn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(rtxn);
    QCOMPARE(rtxn->tag(), 64u);
    rtxn->abort();
}

void TestHBtree::cursors()
{
    QSKIP("cursor copy ctors not implemented yet");

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    txn->put(QByteArray("1"), QByteArray("a"));
    txn->put(QByteArray("2"), QByteArray("b"));
    txn->put(QByteArray("3"), QByteArray("c"));
    txn->put(QByteArray("4"), QByteArray("d"));
    txn->commit(0);

    txn = db->beginRead();

    QByteArray k1, k2;
    HBtreeCursor c1;
    HBtreeCursor c2(txn);

    c2.first();
    c2.current(&k1, 0);
    QCOMPARE(k1, QByteArray("1"));

    c2.next();
    c2.current(&k1, 0);
    QCOMPARE(k1, QByteArray("2"));

    c1 = c2;
    c1.current(&k1, 0);
    c2.current(&k2, 0);
    QCOMPARE(k1, k2);

    c1.next();
    c1.current(&k1, 0);
    c2.current(&k2, 0);
    QCOMPARE(k1, QByteArray("3"));
    QCOMPARE(k2, QByteArray("2"));

    HBtreeCursor c3(c1);
    c3.next();
    c1.current(&k1, 0);
    c3.current(&k2, 0);
    QCOMPARE(k1, QByteArray("3"));
    QCOMPARE(k2, QByteArray("4"));

    txn->abort();
}

void TestHBtree::markerOnReopen_data()
{
    // This test won't work if numCommits results in an auto sync or a split
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::markerOnReopen()
{
    QFETCH(quint32, numCommits);
    const quint32 pageSize = db->pageSize();

    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray("value")));
        QVERIFY(txn->commit(i));
    }

    // First commit goes to marker on page 4
    const quint32 lastMarkerCommited = ((numCommits % 2) == 0 ? 3 : 4);
    quint32 cPage = *d->collectiblePages_.constBegin();

    // There should be 1 reusable page if numCommits is greater than 3
    // Plus the 3 copies of the leaf page
    // Plus 1 header
    // Plus the 4 markers
    // == 9 pages
    QCOMPARE(d->currentMarker().info.number, lastMarkerCommited);
    QCOMPARE(d->collectiblePages_.size(), 1);
    QCOMPARE(d->size_, quint32(pageSize * 9));
    QCOMPARE(d->currentMarker().meta.revision, numCommits);
    QCOMPARE(d->currentMarker().meta.syncedRevision, 1u);

    db->close();
    QVERIFY(db->open());

    QCOMPARE(d->currentMarker().info.number, 3u); // On open markers are synced. Starts with page 3u (ping marker)
    QCOMPARE(d->collectiblePages_.size(), 1);
    QCOMPARE(*d->collectiblePages_.constBegin(), cPage);
    QCOMPARE(d->size_, quint32(pageSize * 9));
    QCOMPARE(d->currentMarker().meta.revision, numCommits);
    QCOMPARE(d->currentMarker().meta.syncedRevision, 1u);
}

void TestHBtree::corruptSinglePage(int psize, int pgno, qint32 type)
{
    const int asize = psize / 4;
    quint32 *page = new quint32[asize];
    QFile::OpenMode om = QFile::ReadWrite;

    if (pgno == -1)  // we'll be appending
        om |= QFile::Append;
    QFile file(dbname);
    QVERIFY(file.open(om));
    QVERIFY(file.seek((pgno == -1 ? 0 : pgno * psize)));
    QVERIFY(file.read((char*)page, asize));

    if (pgno == -1)
        pgno = file.size() / psize; // next pgno
    page[2] = pgno;
    if (type > 0)
        page[1] = type; // set page flag if specified

    for (int j = 3; j < asize; ++j) // randomly corrupt page (skip type and pgno)
        page[j] = rand();

    QVERIFY(file.seek(pgno * psize));
    QCOMPARE(file.write((char*)page, psize), (qint64)psize);
    file.close();

    delete [] page;
}

void TestHBtree::corruptSyncMarker1_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptSyncMarker1()
{
    QFETCH(quint32, numCommits);

    quint32 psize = db->pageSize();
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    QCOMPARE(d->collectiblePages_.size(), 1);
    quint32 cPage = *d->collectiblePages_.constBegin();

    for (int i = 0; i < 5; ++i) {
        db->close();

        corruptSinglePage(psize, 1, HBtreePrivate::PageInfo::Marker);

        QVERIFY(db->open());

        QCOMPARE(d->collectiblePages_.size(), 1);
        QCOMPARE(*d->collectiblePages_.constBegin(), cPage);

        for (quint32 i = 0; i < numCommits; ++i) {
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(txn);
            QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
            txn->abort();
        }
    }
}

void TestHBtree::corruptBothSyncMarkers_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptBothSyncMarkers()
{
    QFETCH(quint32, numCommits);

    quint32 psize = db->pageSize();
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    for (int i = 0; i < 5; ++i) {
        db->close();

        corruptSinglePage(psize, 1, HBtreePrivate::PageInfo::Marker);
        corruptSinglePage(psize, 2, HBtreePrivate::PageInfo::Marker);

        QVERIFY(db->open());
        // Now everything should be synced to ping or pong, whichever was newer and uncorrupted.

        for (quint32 i = 0; i < numCommits; ++i) {
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(txn);
            QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
            txn->abort();
        }
    }
}

void TestHBtree::corruptPingMarker_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptPingMarker()
{
    QFETCH(quint32, numCommits);

    // Ping is marker 3 and gets written every even commit. If commits are even, the last commit won't be there.
    bool includeLast = numCommits % 2;

    quint32 psize = db->pageSize();
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    for (int i = 0; i < 5; ++i) {
        db->close();

        corruptSinglePage(psize, 3, HBtreePrivate::PageInfo::Marker);

        QVERIFY(db->open());
        QCOMPARE(db->tag(), (includeLast ? (numCommits - 1) : (numCommits - 2)));
        // Now everything should be synced to ping or pong, whichever was newer and uncorrupted.

        for (quint32 i = 0; i < numCommits; ++i) {
            if (!includeLast && i == (numCommits - 1))
                continue;
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(txn);
            QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
            txn->abort();
        }
    }
}

void TestHBtree::corruptPongMarker_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptPongMarker()
{
    QFETCH(quint32, numCommits);

    // Pong is marker 4 and gets written every odd commit. If commits are odd, the last commit won't be there.
    bool includeLast = !(numCommits % 2);

    quint32 psize = db->pageSize();
    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    for (int i = 0; i < 5; ++i) {
        db->close();

        corruptSinglePage(psize, 4, HBtreePrivate::PageInfo::Marker);

        QVERIFY(db->open());
        QCOMPARE(db->tag(), (includeLast ? (numCommits - 1) : (numCommits - 2)));
        // Now everything should be synced to ping or pong, whichever was newer and uncorrupted.

        for (quint32 i = 0; i < numCommits; ++i) {
            if (!includeLast && i == (numCommits - 1))
                continue;
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QVERIFY(txn);
            QCOMPARE(txn->get(QByteArray::number(i)), QByteArray::number(i));
            txn->abort();
        }
    }
}


void TestHBtree::corruptPingAndPongMarker_data()
{
    QTest::addColumn<quint32>("numCommits");
    QTest::newRow("Even commits") << 10u;
    QTest::newRow("Odd Commits") << 13u;
}

void TestHBtree::corruptPingAndPongMarker()
{
    QFETCH(quint32, numCommits);

    quint32 psize = db->pageSize();

    HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
    QVERIFY(txn);
    QVERIFY(txn->put(QByteArray("foo"), QByteArray("bar")));
    QVERIFY(txn->commit(42));
    QVERIFY(db->sync());

    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QVERIFY(txn);
        QVERIFY(txn->put(QByteArray::number(i), QByteArray::number(i)));
        QVERIFY(txn->commit(i));
    }

    d->close(false);

    corruptSinglePage(psize, 3, HBtreePrivate::PageInfo::Marker);
    corruptSinglePage(psize, 4, HBtreePrivate::PageInfo::Marker);

    QVERIFY(db->open());
    QCOMPARE(db->tag(), 42u);

    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
        QVERIFY(txn);
        QVERIFY(txn->get(QByteArray::number(i)).isEmpty());
        txn->abort();
    }

    txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
    QVERIFY(txn);
    QCOMPARE(txn->get(QByteArray("foo")), QByteArray("bar"));
    txn->abort();
}

struct CorruptionTestInfo
{
    CorruptionTestInfo(const QList<int> &list)
        : markers(list)
    {}
    QList<int> markers;
};

void TestHBtree::epicCorruptionTest_data()
{
    const quint32 numOddCommits = 13;
    const quint32 numEvenCommits = 10;

    CorruptionTestInfo info[] = {
        CorruptionTestInfo((QList<int>() << 1)),
        CorruptionTestInfo((QList<int>() << 2)),
        CorruptionTestInfo((QList<int>() << 3)),
        CorruptionTestInfo((QList<int>() << 4)),
        CorruptionTestInfo((QList<int>() << 1 << 2)),
        CorruptionTestInfo((QList<int>() << 1 << 3)),
        CorruptionTestInfo((QList<int>() << 1 << 4)),
        CorruptionTestInfo((QList<int>() << 2 << 3)),
        CorruptionTestInfo((QList<int>() << 2 << 4)),
        CorruptionTestInfo((QList<int>() << 3 << 4)),
        CorruptionTestInfo((QList<int>() << 1 << 2 << 3)),
        CorruptionTestInfo((QList<int>() << 1 << 2 << 4)),
        CorruptionTestInfo((QList<int>() << 1 << 3 << 4)),
        CorruptionTestInfo((QList<int>() << 2 << 3 << 4)),
        CorruptionTestInfo((QList<int>() << 1 << 2 << 3 << 4)),
    };

    const int numItems = sizeof(info) / sizeof(CorruptionTestInfo);

    QTest::addColumn<quint32>("numCommits");
    QTest::addColumn<QList<int> >("markers");
    QTest::addColumn<bool>("openable");

    for (int i = 0; i < numItems; ++i) {
        QString evenTag = QString("Even commits corrupted m");
        QString oddTag = QString("Odd commits corrupted m");
        foreach (int j, info[i].markers) {
            evenTag += QString::number(j);
            oddTag += QString::number(j);
        }
        QTest::newRow(evenTag.toAscii()) << numEvenCommits << info[i].markers;
        QTest::newRow(oddTag.toAscii()) << numOddCommits << info[i].markers;
    }
}

void TestHBtree::epicCorruptionTest()
{
    db->setAutoSyncRate(0);
    QFETCH(quint32, numCommits);
    QFETCH(QList<int>, markers);

    const int appendCount = 100;
    const int psize = db->pageSize();
    const quint32 halfway = numCommits / 2;
    QList<QByteArray> beforeSync;
    QList<QByteArray> afterSync;

    for (quint32 i = 0; i < numCommits; ++i) {
        HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
        QByteArray key = QByteArray::number(i);
        QVERIFY(txn);
        QVERIFY(txn->put(key, key));
        QVERIFY(txn->commit(i));

        if (i == halfway)
            db->sync();

        if (i > halfway)
            afterSync.append(key);
        else
            beforeSync.append(key);
    }

    d->close(false);

    bool hasSync1 = true;
    bool hasSync2 = true;
    bool hasPing = true;
    bool hasPong = true;
    foreach (int pgno, markers) {
        if (pgno == 1)
            hasSync1 = false;
        if (pgno == 2)
            hasSync2 = false;
        if (pgno == 3)
            hasPing = false;
        if (pgno == 4)
            hasPong = false;
        corruptSinglePage(psize, pgno, HBtreePrivate::PageInfo::Marker);
    }

    bool hasBeforeSync = hasSync1 || hasSync2 || hasPing || hasPong;
    bool hasAfterSync = hasPing || hasPong;
    bool openable = hasBeforeSync || hasAfterSync;
    bool hasLastCommit = ((numCommits % 2) == 0 && hasPing) || ((numCommits % 2) == 1 && hasPong);

    QCOMPARE(db->open(), openable);

    if (!hasAfterSync && hasBeforeSync)
        QCOMPARE(db->tag(), (quint32)halfway);
    else if (hasAfterSync)
        QCOMPARE(db->tag(), (hasLastCommit ? numCommits - 1 : numCommits  - 2));

    if (openable) {

        for (quint32 i = numCommits; i < numCommits + appendCount; ++i) {
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadWrite);
            QByteArray key = QByteArray::number(i);
            QVERIFY(txn);
            QVERIFY(txn->put(key, key));
            QVERIFY(txn->commit(i));
        }

        if (hasBeforeSync) {
            for (int i = 0; i < beforeSync.size(); ++i) {
                HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
                QVERIFY(txn);
                QCOMPARE(txn->get(beforeSync[i]), beforeSync[i]);
                txn->abort();
            }
        }

        if (hasAfterSync) {
            for (int i = 0; i < afterSync.size(); ++i) {
                if (!hasLastCommit && i == (afterSync.size() - 1))
                    continue;
                HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
                QVERIFY(txn);
                QCOMPARE(txn->get(afterSync[i]), afterSync[i]);
                txn->abort();
            }
        }

        for (quint32 i = numCommits; i < numCommits + appendCount; ++i) {
            HBtreeTransaction *txn = db->beginTransaction(HBtreeTransaction::ReadOnly);
            QByteArray key = QByteArray::number(i);
            QVERIFY(txn);
            QCOMPARE(txn->get(key), key);
            txn->abort();
        }
    }
}

void TestHBtree::orderedList()
{
    OrderedList<HBtreePrivate::NodeKey, HBtreePrivate::NodeValue> list;

    typedef HBtreePrivate::NodeKey Key;
    typedef HBtreePrivate::NodeValue Value;

    Key key;

    key = Key(0, QByteArray("B"));
    list.insert(key, Value("_B_"));

    key = Key(0, QByteArray("A"));
    list.insert(key, Value("_A_"));

    key = Key(0, QByteArray("C"));
    list.insert(key, Value("_C_"));

    QCOMPARE(list.size(), 3);
    QVERIFY((list.constBegin() + 0).key().data == QByteArray("A"));
    QVERIFY((list.constBegin() + 1).key().data == QByteArray("B"));
    QVERIFY((list.constBegin() + 2).key().data == QByteArray("C"));

    QVERIFY(list.contains(Key(0, "A")));
    QVERIFY(!list.contains(Key(0, "AA")));
    QVERIFY(!list.contains(Key(0, "D")));

    QVERIFY(list.lowerBound(Key(0, "A")) == list.constBegin());
    QVERIFY(list.lowerBound(Key(0, "AA")) == list.constBegin()+1);
    QVERIFY(list.lowerBound(Key(0, "B")) == list.constBegin()+1);
    QVERIFY(list.lowerBound(Key(0, "D")) == list.constEnd());

    QVERIFY(list.upperBound(Key(0, "A")) == list.constBegin()+1);
    QVERIFY(list.upperBound(Key(0, "AA")) == list.constBegin()+1);
    QVERIFY(list.upperBound(Key(0, "B")) == list.constBegin()+2);
    QVERIFY(list.upperBound(Key(0, "D")) == list.constEnd());

    QCOMPARE(list.size(), 3);
    QVERIFY(list[Key(0, "C")].data == QByteArray("_C_"));
    QCOMPARE(list.size(), 3);
    list[Key(0, "C")].data = QByteArray("_C2_");
    QVERIFY(list[Key(0, "C")].data == QByteArray("_C2_"));
    QCOMPARE(list.size(), 3);
    QVERIFY(list[Key(0, "AA")].data.isEmpty());
    QVERIFY(list.contains(Key(0, "AA")));
    QCOMPARE(list.size(), 4);

    QVERIFY(list.find(Key(0, "D")) == list.constEnd());

    QCOMPARE(list.size(), 4);
    list.insert(Key(0, "B"), Value("_B2_"));
    QCOMPARE(list.size(), 4);

    QCOMPARE(list.value(Key(0, "A")).data, QByteArray("_A_"));
    QCOMPARE(list.value(Key(0, "B")).data, QByteArray("_B2_"));
    QCOMPARE(list.value(Key(0, "C")).data, QByteArray("_C2_"));
    QCOMPARE(list.value(Key(0, "AA")).data, QByteArray(""));
}

QTEST_MAIN(TestHBtree)
#include "main.moc"
