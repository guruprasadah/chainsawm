SOURCES = chainsawm.cxx
TARGET = chainsawm
CXX = g++
CXXFLAGS = -Wall $(shell pkg-config x11 --cflags --libs) $(shell pkg-config xcursor --cflags --libs) -g

all : clean $(TARGET)

$(TARGET) :
	$(CXX) $(SOURCES) -o $(TARGET) $(CXXFLAGS)

clean:
	rm $(TARGET)

install:
	cp $(TARGET) /usr/bin/$(TARGET)
