CXXFLAGS     = -O2 -std=c++11 -I..
CXXFLAGS    += -Wall -pedantic -Wextra -D_XOPEN_SOURCE=700 
DEPS         = ../sho.h
TARGETS      = test_sho


ifeq ($(OS),Windows_NT)
    LDFLAGS  = -lpsapi
endif

all: $(TARGETS)

clean:
	rm -rf $(TARGETS) 

test:
	./test_sho

%: %.cc $(DEPS) makefile
	$(CXX) $(CXXFLAGS) -DNDEBUG $< -o $@ $(LDFLAGS)

