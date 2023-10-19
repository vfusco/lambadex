CXX = g++
CXXFLAGS = -std=c++17

SRCS = exchange.cpp state-mgr.cpp
OBJS = $(SRCS:.cpp=.o)

# Generic rule to build .o file from .cpp
%.o: %.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c $< -o $@

exchange: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
