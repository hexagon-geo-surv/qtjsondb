INCLUDEPATH += $$PWD

unix {
    contains(QT_CONFIG,icu) {
        LIBS += -licuuc -licui18n
    } else {
        DEFINES += NO_COLLATION_SUPPORT
    }
}

HEADERS += \
    $$PWD/jsondb-global.h \
    $$PWD/jsondb-error.h \
    $$PWD/jsondb-strings.h \
    $$PWD/jsondbcollator.h \
    $$PWD/jsondbcollator_p.h

SOURCES += \
    $$PWD/jsondb-strings.cpp \
    $$PWD/jsondbcollator.cpp
