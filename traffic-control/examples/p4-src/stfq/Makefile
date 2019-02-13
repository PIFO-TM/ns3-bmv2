
ARCH_DIR=${NS3_DIR}/src/p4-pipeline/p4-architectures
TOOLS_DIR=${NS3_DIR}/src/bmv2-tools

all:
	cp ${ARCH_DIR}/classification_pipe.p4 ./
	cp ${ARCH_DIR}/enq_pipe.p4 ./
	cp ${ARCH_DIR}/deq_pipe.p4 ./
	cp ${TOOLS_DIR}/dummy_blocks.p4 ./
	bash ${TOOLS_DIR}/compile-p4.sh classification/class
	bash ${TOOLS_DIR}/compile-p4.sh node0/enq
	bash ${TOOLS_DIR}/compile-p4.sh node0/deq

clean:
	rm -f classification/*.json node0/*.json
	rm -r dummy_blocks.p4
	rm -f *_pipe.p4

