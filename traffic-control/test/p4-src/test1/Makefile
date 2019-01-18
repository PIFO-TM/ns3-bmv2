
all:
	cp ${NS3_DIR}/src/p4-pipeline/p4-architectures/classification_pipe.p4 ./
	bash compile-p4.sh class
	cp ${NS3_DIR}/src/p4-pipeline/p4-architectures/enq_pipe.p4 ./
	bash compile-p4.sh enq
	cp ${NS3_DIR}/src/p4-pipeline/p4-architectures/deq_pipe.p4 ./
	bash compile-p4.sh deq

clean:
	rm class.json enq.json deq.json

