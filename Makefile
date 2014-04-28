# You can use following macros:
#   - USE_GLPK : For GNU Linear Programming Kit
#   - USE_LP_SOLVE : For LP-Solve
#   - USE_GUROBI : For Gurobi optimizer

USE_GUROBI = no
USE_LP_SOLVE = yes

TARGET = ./bin/phil
CXX = g++
OPTS = -O2 -std=c++11 -g -DDISABLE_CUTTING_LHS
#OPTS = -O2 -std=c++11 -g

SOURCE = $(shell find src -name *.cpp)
HEDDER = $(shell find src -name *.h)
OBJS = $(SOURCE:.cpp=.o)

IDFLAGS =
LDFLAGS =

ifeq ($(USE_LP_SOLVE), yes)
OPTS += -DUSE_LP_SOLVE
IDFLAGS += -I$(LPSOLVE_HOME)
LDFLAGS += -L$(LPSOLVE_HOME) -llpsolve55
endif

ifeq ($(USE_GUROBI), yes)
OPTS += -DUSE_GUROBI
IDFLAGS += -I$(GUROBI_HOME)/include
LDFLAGS += -L$(GUROBI_HOME)/lib -lgurobi_c++ -lgurobi56 -lpthread
endif


all: $(OBJS)
	mkdir -p ./bin
	$(CXX) $(OPTS) $(OBJS) $(IDFLAGS) $(LDFLAGS) -o $(TARGET)

.cpp.o:
	$(CXX) $(OPTS) $(IDFLAGS) -c -o $(<:.cpp=.o) $<

clean:
	rm -f $(TARGET)
	rm -f $(OBJS)

tar:
	tar cvzf $(TARGET).tar.gz $(SOURCE) $(HEDDER) Makefile

test:
	bin/phil -l conf/toy.compile.conf
	bin/phil -l conf/toy.inference.conf
