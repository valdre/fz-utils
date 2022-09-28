### Simone Valdre' - 22/12/2020. Distributed under GPL-3.0-or-later licence

all: classes fz-acqui fz-meter fz-tree fz-test
	

classes:
	$(MAKE) -C modules all

modules/*.o: classes
	

fz-tree: main/fz-tree.cpp modules/Fzpbread.o modules/FzEventSet.pb.o
	g++ -Wall -Wextra $^ -Imodules -lprotobuf -fopenmp `root-config --cflags --glibs` -o $@

fz-acqui: main/fz-acqui.cpp
	g++ -Wall -Wextra $^ `root-config --cflags --glibs` -o $@

fz-meter: main/fz-meter.cpp modules/FzMeterDict.cxx modules/FzSC.o
	g++ -Wall -Wextra $^ -Imodules -lm `root-config --cflags --glibs` -o $@

fz-test: main/fz-test.cpp modules/FzSC.o modules/FzTestCore.o modules/FzTestUI.o
	g++ -Wall -Wextra $^ -Imodules -o $@

clean:
	rm -f fz-* *.pcm modules/*.pb.* modules/*.o modules/*Dict.* *.d *.pcm modules/*.pcm AutoDict*
