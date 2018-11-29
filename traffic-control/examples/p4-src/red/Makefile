
SRC=red

all:
	cp ../../simple_pipe.p4 ./
	sudo docker run -w /p4c/build -v $(shell pwd):/tmp p4c p4c-bm2-ss -I/tmp --p4v 16 -o /tmp/${SRC}.json /tmp/${SRC}.p4
	sudo chown sibanez:sibanez ${SRC}.json

clean:
	rm ${SRC}.json

