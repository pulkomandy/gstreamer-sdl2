GST=`pkg-config --cflags --libs gstreamer-0.10` -lgstapp-0.10
SDL=`sdl2-config --libs`

a.out: main.cpp
	$(CXX) -Wall -Wextra main.cpp $(SDL) $(GST) -std=c++11 $(LDFLAGS)
