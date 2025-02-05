CXX = g++
CXXFLAGS = -std=c++17 -Wall -I/home/kiyun/lib_kiyun/cpp-httplib-master -I/home/kiyun/lib_kiyun
LDFLAGS = -lpugixml -lsqlite3
TARGET = main
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp header.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
