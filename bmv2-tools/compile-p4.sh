#!/bin/bash

PROG_NAME=${1}

sudo docker run -w /p4c/build -v ${PWD}:/tmp p4c p4c-bm2-ss -I/tmp --p4v 16 -o /tmp/${PROG_NAME}.json /tmp/${PROG_NAME}.p4
sudo chown sibanez:sibanez ${PROG_NAME}.json

