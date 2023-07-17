#!/bin/sh

make clean

make test

if [ $? -eq 0 ]
then
	echo "================ TEST ================"
	./test/test
fi
