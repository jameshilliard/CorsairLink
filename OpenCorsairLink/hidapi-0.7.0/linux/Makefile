###########################################
# Simple Makefile for HIDAPI test program
#
# Alan Ott
# Signal 11 Software
# 2010-06-01
###########################################

all: hidtest

CC       ?= gcc
CFLAGS   ?= -Wall -g -pthread

CXX      ?= g++
CXXFLAGS ?= -Wall -g hid-libusb.o

COBJS     = hid-libusb.o
CPPOBJS   = ../hidtest/hidtest.o
OBJS      = $(COBJS) $(CPPOBJS)
LIBS      = `pkg-config libusb-1.0 libudev --libs`
INCLUDES ?= -I../hidapi `pkg-config libusb-1.0 --cflags`


hidtest: $(OBJS)
	$(CXX) $(CXXFLAGS) -pthread $(LDFLAGS) $^ $(LIBS) -o hidtest

$(COBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

$(CPPOBJS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) hidtest

.PHONY: clean
