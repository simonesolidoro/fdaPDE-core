CXX = g++-14 
CXXFLAGS = -std=c++20 -pthread -I/home/simo/eigen-3.4.0 -I../../../../../  
SOURCES = $(wildcard test*.cpp) 
EXECUTABLES = $(SOURCES:.cpp=) 

all: $(EXECUTABLES)


%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@


clean:
	rm -f $(EXECUTABLES)
rmtxt:
	rm *.txt
