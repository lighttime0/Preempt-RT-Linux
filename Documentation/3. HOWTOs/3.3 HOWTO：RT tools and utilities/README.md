# README

> 原网页地址：https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/start

## 3.3 HOWTO: RT tools and utilities

下面是一些工具和使用程序，可以帮助你编写用户空间的应用程序或者对rt kernel进行修改。

<!-- Following are the tools and utilities which might be helpful to someone who is trying to write an userspace application or make changes to the real-time kernel: -->

### Test Suites

下面是一些测试工具：

<!-- There are several test suites available: -->

* [Linux Test Project](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/ltp)

    详见：3.3.1.1 Linux Test Project (LTP)

* [rt-tests](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/rt-tests)

    详见：3.3.1.2 RT-Tests

### Latency detection

下面的工具可以用来检测延迟：

<!-- Latency can be detected with the following tool(s): -->

* [cyclictest](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/cyclictest) (part of the test suite [rt-tests](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/rt-tests))

    详见：3.3.2.1 Cyclictest

下面的projects展示了使用cyclictest的样例程序：

<!-- Application examples of cyclictest show the following projects: -->

* [rteval](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/rteval)

    详见：3.3.2.2 RTEval

* [Worst case latency test scenarios](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/worstcaselatency)

    详见：3.3.2.3 Worst Case Latency Test Scenarios

批注： cyclictest 是什么？
Cyclictest的维基主页这么介绍它：“Cyclictest is a high resolution test program, written by User:Tglx, maintained by User:Clark Williams ”，就是说它是一个高精度的测试程序，Cyclictest 是 rt-tests 下的一个测试工具，也是rt-tests 下使用最广泛的测试工具，一般主要用来测试使用内核的延迟，从而判断内核的实时性。

### Resource Partitioning

* [CPU Partitioning](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/cpu-partitioning/start)

    详见：3.3.3.1 CPU Partitioning

