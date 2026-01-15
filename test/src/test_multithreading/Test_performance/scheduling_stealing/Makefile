CXX = g++-14 
CXXFLAGS = -std=c++20 -pthread -I../../../../../ -fopenmp 
SOURCES = $(wildcard test*.cpp) 
EXECUTABLES = $(SOURCES:.cpp=) 

all: $(EXECUTABLES)


%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@


clean:
	rm -f $(EXECUTABLES)
rmtxt:
	rm *.txt
