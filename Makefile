all:
	g++ -std=c++11 -O3 -o bin/vf3p2new main.cpp -DVF3PV2 -Iinclude -lpthread
	g++ -std=c++11 -O3 -o bin/vf3p1new main.cpp -DVF3PV1 -Iinclude -lpthread
	g++ -std=c++11 -O3 -o bin/vf3l main.cpp -DVF3L -Iinclude -lpthread

clean:
	rm bin/*