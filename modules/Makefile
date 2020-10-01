all: FzEventSet.pb.o Fzpbread.o FzSC.o FzMeterDict.cxx
	

FzEventSet.pb.cpp: FzEventSet.proto
	protoc FzEventSet.proto --cpp_out=.
	mv FzEventSet.pb.cc FzEventSet.pb.cpp
	sed -i 's/assert/\/\/assert/' FzEventSet.pb.h

.cpp.o: FzEventSet.pb.cpp
	g++ -Wall -Wextra -c -o $@ $<

FzMeterDict.cxx: FzMeter.h FzMeterLinkDef.h
	rootcling -f FzMeterDict.cxx -c FzMeter.h FzMeterLinkDef.h
