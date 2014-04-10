# You can use following macros:
#   - USE_GLPK : For GNU Linear Programming Kit
#   - USE_LP_SOLVE : For LP-Solve
#   - USE_GUROBI : For Gurobi optimizer

TARGET = ./bin/phil
CXX = g++
OPTS = -O2 -std=c++11 -g
LDFLAGS = -Lsrc/lib/lpsolve55

SOURCE = $(shell find src -name *.cpp)
HEDDER = $(shell find src -name *.h)
OBJS = $(SOURCE:.cpp=.o)

MACROS = -DDISABLE_CUTTING_LHS

all: $(OBJS)
	mkdir -p ./bin
	$(CXX) $(OPTS) $(OBJS) $(LDFLAGS) -o $(TARGET)

.cpp.o:
	$(CXX) $(OPTS) -c -o $(<:.cpp=.o) $<

clean:
	rm -f $(TARGET)
	rm -f $(OBJS)

tar:
	tar cvzf $(TARGET).tar.gz $(SOURCE) $(HEDDER) Makefile
