CROSS_COMPILE ?= aarch64-none-linux-gnu-
CXX = $(CROSS_COMPILE)g++

TARGET = Peripheral_interface_test
SOURCES = main.cpp WifiInterface.cpp BlueInterface.cpp
CXXFLAGS = -Wall -std=c++11 -O2 
LDFLAGS = -lpthread

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET) Peripheral_interface_test

.PHONY: all clean