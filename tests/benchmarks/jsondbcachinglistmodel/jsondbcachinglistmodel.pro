TEMPLATE = app
TARGET = tst_bench_jsondbcachinglistmodel
DEPENDPATH += .
INCLUDEPATH += .

QT = core network testlib gui qml jsondbcompat-private
CONFIG -= app_bundle
CONFIG += testcase

include($$PWD/../../shared/shared.pri)

DEFINES += JSONDB_DAEMON_BASE=\\\"$$QT.jsondb.bins\\\"
DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += jsondbcachinglistmodel-bench.h
SOURCES += jsondbcachinglistmodel-bench.cpp
