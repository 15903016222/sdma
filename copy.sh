#!/bin/sh

# if [ ! -f mxc_sdma_memcopy_test.ko ]; then
# if [ ! -f dma.ko ]; then
if [ ! -f dma_sg.ko ]; then
	echo "the dma's driver is not builded."
	exit
else
#	scp dma_drv.ko root@192.168.1.215:/tmp
#	scp dma_dev.ko root@192.168.1.215:/tmp
#	scp insmod.sh  root@192.168.1.215:/tmp
# 	scp mxc_sdma_memcopy_test.ko root@192.168.1.224:/tmp
	scp mxc_test root@192.168.1.224:/tmp
	scp dma_sg.ko root@192.168.1.224:/tmp
#	scp dma.ko root@192.168.1.224:/tmp
fi
