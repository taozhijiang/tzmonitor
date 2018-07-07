IDLS=$(wildcard *.thrift)
OBJS=$(IDLS:%.thrift=../gen-cpp/%.thrift.cc)

all: ${OBJS}
	@echo "remove ../gen-cpp/*.skeleton.cpp files"
	rm -fr ../gen-cpp/*.skeleton.cpp

../gen-cpp/%.thrift.cc: %.thrift
	../../../bin/thrift -r -gen cpp -out $(@D) $^
