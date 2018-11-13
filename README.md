
# Installation

* Install [bmv2](https://github.com/p4lang/behavioral-model)
* Install NS3 dependencies listed [here](https://www.nsnam.org/wiki/Installation#Installation)
* Follow the installation steps below:
```
$ cd
$ mkdir ns3-repos
$ cd ns3-repos
$ hg clone http://code.nsnam.org/ns-3-allinone
$ ./download.py -n ns-3.29
$ cd <ns3-directory>
$ ./waf configure --enable-tests --enable-examples
$ ./waf build
```

* Clone this repo:
```
$ cd
$ git clone <repo-link>
$ cd <repo-dir>
$ <install source files into NS3 src directory>
$ cd ~/ns3-repos/<ns3->
``` 

