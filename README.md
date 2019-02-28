
### tzmonitor

***tzmonitor*** is a high performance and easily used runtime information collecting and storing system. Though it can not compare with other monitoring systems for maturity, stability, scaling and beauty, but I believe tzmonitor is suitable for some specific situations. The most emphasized point is the easily historical information selection, which is very important for feedback-support system, lots of business runtime information is not only for display, but can help the system make decisions smartly and automatically.   
 
The key points of tzmonitor’s design and implementation includes:   
1. Friendly and easily use interface, support both report and select functions. We supported client library and single header file is enough.   
2. Whole system is built on [tzrpc](https://github.com/taozhijiang/tzrpc), which is a high performance RPC framework based on boost.asio and protobuf.   
3. Can dynamically scale-up and scale-down threadpool size based on overloads, and some parameters support dynamically updating.   
4. Mysql and LevelDB storage methods are currently supported, and others can be easily added by implementing specified methods.   
5. A simple [tzhttpd](https://github.com/taozhijiang/tzhttpd) drivend http_face web interface, though currently not beautiful enough.   
