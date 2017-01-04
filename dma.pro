TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    dma_cyclic.c \
    dma_sg.c \
    mxc_test.c

INCLUDEPATH += ../../linux/include
