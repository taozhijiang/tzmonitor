
g++ -std=c++0x -O0 -g -DTZMONITOR_CLIENT main.cpp -I../../xtra_rhel6.x/include/ -L../../xtra_rhel6.x/libs/release/boost/ -Llibs/ -lconfig++ -ltzmonitor_client -ljsoncpp -lthrifting -lthriftz -lthrift -lcurl -lboost_system -o client_test

g++ -std=c++0x -O0 -g -DTZMONITOR_CLIENT main2.cpp -I../../xtra_rhel6.x/include/ -L../../xtra_rhel6.x/libs/release/boost/ -Llibs/ -lconfig++ -ltzmonitor_client -ljsoncpp -lthrifting -lthriftz -lthrift -lcurl -lboost_system -o client_test2
