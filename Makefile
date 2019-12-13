all: classes fz-acqui fz-meter fz-tree
	

classes:
	$(MAKE) -C modules all

modules/*.o: classes
	

fz-tree: main/fz-tree.cpp modules/Fzpbread.o modules/FzEventSet.pb.o
	g++ -Wall -Wextra $^ -Imodules -lprotobuf -fopenmp `root-config --cflags --glibs` -o $@

fz-acqui: main/fz-acqui.cpp
	g++ -Wall -Wextra $^ `root-config --cflags --glibs` -o $@

fz-meter: main/fz-meter.cpp modules/FzMeterDict.cxx modules/FzUDP.o
	g++ -Wall -Wextra $^ -Imodules -lm `root-config --cflags --glibs` -o $@

clean:
	rm -f fz-* modules/*.pb.* modules/*.o modules/*Dict.* *.d *.pcm AutoDict*
