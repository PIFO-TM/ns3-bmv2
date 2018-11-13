
# Installation

* Install [bmv2](https://github.com/p4lang/behavioral-model)
* Install NS3 dependencies listed [here](https://www.nsnam.org/wiki/Installation#Installation)
* Follow the installation steps below:
```
$ cd
$ mkdir ns3-repos
$ cd ns3-repos
$ hg clone http://code.nsnam.org/ns-3-allinone
$ cd ns-3-allinone
$ ./download.py -n ns-3.29
$ cd ns-3.29/
Optional:
$ ./waf configure --enable-tests --enable-examples
$ ./waf build
```

* Clone this repo and install it on top of the NS3 source files:
```
$ cd ~/ns3-repos/
$ git clone https://github.com/PIFO-TM/ns3-bmv2.git
$ cd ns3-bmv2/
NOTE: This next step is very hacky and should be fixed in the future ...
Update the NS3_SRC_DIR in the install.sh script then run:
$ bash install.sh
$ cd ~/ns3-repos/ns-3-allinone/ns-3.29/
$ ./waf configure --enable-tests --enable-examples
$ ./waf build
``` 

