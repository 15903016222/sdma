# demo:
#	$(CC) mxc_test.c -o mxc_test

# obj-m +=mxc_sdma_memcopy_test.o
# obj-m +=dma.o
obj-m +=dma_sg.o
# obj-m +=m2m.o
all:
	make -C /home/ws/kernel/linux SUBDIRS=$(PWD) modules
clean:
	make -C /home/ws/kernel/linux SUBDIRS=$(PWD) clean 
