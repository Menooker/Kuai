SRC_DIR=$(shell pwd)/../src/
CXXFLAGS:=-I${SRC_DIR} -std=c++11 -O2
BIN_DIR=$(shell pwd)/bin
SRC=${SRC_DIR}/Kuai/*.hpp

all: ${BIN_DIR}/main ${BIN_DIR}/benchmark
${BIN_DIR}:
	mkdir -p ${BIN_DIR}

${BIN_DIR}/main.o: ${SRC} ${BIN_DIR} main.cpp
	g++ -c ${CXXFLAGS} main.cpp -o ${BIN_DIR}/main.o

${BIN_DIR}/main: ${BIN_DIR}/main.o
	g++ ${CXXFLAGS} -pthread ${BIN_DIR}/main.o -o ${BIN_DIR}/main

${BIN_DIR}/benchmark:  ${SRC} ${BIN_DIR} benchmark.cpp
	g++ ${CXXFLAGS} -pthread benchmark.cpp -o ${BIN_DIR}/benchmark

.PHONY: benchtbb
benchtbb: ${BIN_DIR}/benchmark-tbb

${BIN_DIR}/benchmark-tbb:  ${SRC} ${BIN_DIR} benchmark.cpp
	g++ ${CXXFLAGS} -DBENCH_TBB -pthread benchmark.cpp -ltbb -o ${BIN_DIR}/benchmark-tbb