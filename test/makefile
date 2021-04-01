SRC_DIR=$(shell pwd)/../src/
CXXFLAGS:=-I${SRC_DIR} -std=c++11 -O2
BIN_DIR=$(shell pwd)/bin
SRC=${SRC_DIR}/Kuai/*.hpp

all: ${BIN_DIR}/main ${BIN_DIR}/benchmark
${BIN_DIR}:
	mkdir -p ${BIN_DIR}

${BIN_DIR}/main.o: ${SRC} ${BIN_DIR}
${BIN_DIR}/benchmark.o : ${SRC} ${BIN_DIR}

${BIN_DIR}/main: main.o 
	g++ ${CXXFLAGS} -pthread main.o -o ${BIN_DIR}/main

${BIN_DIR}/benchmark: benchmark.o 
	g++ ${CXXFLAGS} -pthread benchmark.o -o ${BIN_DIR}/benchmark
