
g++ -std=c++0x -O0 -g -DTZMONITOR_CLIENT main.cpp -Llibs/ -lconfig++ -ltzmonitor_client -ljson -lthrifting -lthriftnb -lthriftz -lthrift -lcurl -lboost_system -o client_test

g++ -std=c++0x -O0 -g -DTZMONITOR_CLIENT main2.cpp -Llibs/ -lconfig++ -ltzmonitor_client -ljson -lthrifting -lthriftnb -lthriftz -lthrift -lcurl -lboost_system -o client_test2
