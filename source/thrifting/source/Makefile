IDLS=$(wildcard *.thrift)
OBJS=$(IDLS:%.thrift=../gen-cpp/%.thrift.cc)

all: ${OBJS}

../gen-cpp/%.thrift.cc: %.thrift
	../../../bin/thrift -r -gen cpp -out $(@D) $^
