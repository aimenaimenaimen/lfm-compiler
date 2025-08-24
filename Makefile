.PHONY: clean all

# LLVM via Homebrew (Intel macOS)
LLVM_CONFIG = /usr/local/opt/llvm/bin/llvm-config
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core)

CXX = clang++
CXXFLAGS = -std=c++17 -I/usr/local/include -I/usr/local/opt/llvm/include $(LLVM_CXXFLAGS) -fexceptions
LDFLAGS = $(LLVM_LDFLAGS)

OBJS = driver.o parser.o scanner.o lfmc.o IO.o
TARGET = lfmc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

lfmc.o: lfmc.cpp driver.hpp
	$(CXX) -c lfmc.cpp -o $@ $(CXXFLAGS)

parser.o: parser.cpp
	$(CXX) -c parser.cpp -o $@ $(CXXFLAGS)

scanner.o: scanner.cpp parser.hpp
	$(CXX) -c scanner.cpp -o $@ $(CXXFLAGS)

driver.o: driver.cpp parser.hpp driver.hpp
	$(CXX) -c driver.cpp -o $@ $(CXXFLAGS)

IO.o: IO.cpp
	$(CXX) -c IO.cpp -o $@ $(CXXFLAGS)

parser.cpp parser.hpp: parser.yy 
	bison -o parser.cpp parser.yy

scanner.cpp: scanner.ll
	flex -o scanner.cpp scanner.ll

clean:
	rm -f *~ *.o $(TARGET) scanner.cpp parser.cpp parser.hpp
