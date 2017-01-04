/*
 * Copyright 2006-2012 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <mach/dma.h>

#include <linux/dmaengine.h>
#include <linux/device.h>

#include <linux/io.h>
#include <linux/delay.h>
//#include <sys/time.h>

#define DEBUG
#define DEF_SG_QTY 3
#ifdef DEBUG
/* If you are writing a driver, please use dev_dbg instead */
	#define dev_printk(fmt, args...)  printk(KERN_DEBUG fmt, ## args)
#else
	static /*inline*/ int __attribute__ ((format (printf, 1, 2))) dev_printk(const char * fmt, ...)
	{
		return 0;
	}
#endif

static int gMajor; /* major number of device */
static struct class *dma_tm_class;
u32 *wbuf[DEF_SG_QTY];
u32 *rbuf[DEF_SG_QTY];

struct dma_chan *dma_m2m_chan;

struct completion dma_m2m_ok;

struct scatterlist sg[DEF_SG_QTY], sg2[DEF_SG_QTY];

#define SDMA_BUF_SIZE  40960



static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;
	chan->private = param;
	return true;
}

int sdma_open(struct inode * inode, struct file * filp)
{
	int i;
	dma_cap_mask_t dma_m2m_mask;
	struct imx_dma_data m2m_dma_data = {0};

	init_completion(&dma_m2m_ok);

	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_SLAVE, dma_m2m_mask);
	m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
	m2m_dma_data.priority = DMA_PRIO_HIGH;

	dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!dma_m2m_chan) {
		dev_printk("Error opening the SDMA memory to memory channel\n");
		return -EINVAL;
	}

	for(i=0; i<DEF_SG_QTY; i++){
		wbuf[i] = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
		if(!wbuf[i]) {
			dev_printk("error wbuf[%d] !!!!!!!!!!!\n", i);
			return -1;
		}
	}

	dev_printk("kzalloc %d wbuf succeed\n", DEF_SG_QTY);






	for(i=0; i<DEF_SG_QTY; i++){
		rbuf[i] = kzalloc(SDMA_BUF_SIZE, GFP_DMA);
		if(!rbuf[i]) {
			dev_printk("error rbuf[%d] !!!!!!!!!!!\n", i);
			return -1;
		}
	}
	dev_printk("kzalloc %d rbuf succeed\n", DEF_SG_QTY);
	return 0;
}

int sdma_release(struct inode * inode, struct file * filp)
{
	int i;
	dma_release_channel(dma_m2m_chan);
	dma_m2m_chan = NULL;

	for(i=0; i<DEF_SG_QTY; i++)
	{
		kfree(wbuf[i]);
		kfree(rbuf[i]);
	}

	return 0;
}

ssize_t sdma_read (struct file *filp, char __user * buf, size_t count,
								loff_t * offset)
{
	int i,j;

	for (i=0; i<SDMA_BUF_SIZE/4; i++) {
		for(j=0; j<DEF_SG_QTY; j++){
			if(*(rbuf[j]+i) != *(wbuf[j]+i)){
				dev_printk("buf[%d]: %x -- %x\n", i, *(rbuf[j]+i), *(wbuf[j]+i));
			}
		}
		
	}

	dev_printk("buffer 1 copy passed!\n");

	return 0;
}

static void dma_m2m_callback(void *data)
{
	dev_printk("  *** [%s]\n",__func__);
	complete(&dma_m2m_ok);
	return ;
}

ssize_t sdma_write(struct file * filp, const char __user * buf, size_t count,
								loff_t * offset)
{
	u32 *index[3], i, ret, cnt;
	long start, end;
	struct dma_slave_config dma_m2m_config;
	struct dma_async_tx_descriptor *dma_m2m_desc;
	struct timeval end_time;

	for(cnt=0; cnt<DEF_SG_QTY; cnt++){
		index[cnt] = wbuf[cnt];
	}


	for (i=0; i<SDMA_BUF_SIZE/4; i++) {
		*(index[0] + i) = 0x12121212;
	}

	for (i=0; i<SDMA_BUF_SIZE/4; i++) {
		*(index[1] + i) = 0x34343434;
	}

	for (i=0; i<SDMA_BUF_SIZE/4; i++) {
		*(index[2] + i) = 0x56565656;
	}


	dma_m2m_config.direction = DMA_MEM_TO_MEM;
	dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dmaengine_slave_config(dma_m2m_chan, &dma_m2m_config);

	dev_printk(KERN_ERR "%s:%d\n", __func__, __LINE__);
	sg_init_table(sg, DEF_SG_QTY);
	for(cnt=0; cnt<DEF_SG_QTY; cnt++){
		sg_set_buf(&sg[cnt], wbuf[cnt], SDMA_BUF_SIZE);
	}
	ret = dma_map_sg(NULL, sg, DEF_SG_QTY, dma_m2m_config.direction);

	dev_printk(KERN_ERR "%s:%d\n", __func__, __LINE__);
	dma_m2m_desc = dma_m2m_chan->device->device_prep_slave_sg(dma_m2m_chan,sg, DEF_SG_QTY, dma_m2m_config.direction, 1);
	dev_printk(KERN_ERR "%s:%d first m2m_desc=%p\n", __func__, __LINE__, dma_m2m_desc);

	dev_printk(KERN_ERR "%s:%d\n", __func__, __LINE__);

	sg_init_table(sg2, DEF_SG_QTY);
	for(cnt=0; cnt<DEF_SG_QTY; cnt++){
		sg_set_buf(&sg2[cnt], rbuf[cnt], SDMA_BUF_SIZE);
	}

	ret = dma_map_sg(NULL, sg2, DEF_SG_QTY, dma_m2m_config.direction);
	dev_printk(KERN_ERR "%s:%d ret=%d\n", __func__, __LINE__, ret);

	dma_m2m_desc = dma_m2m_chan->device->device_prep_slave_sg(dma_m2m_chan,sg2, DEF_SG_QTY, dma_m2m_config.direction, 0);
	dev_printk(KERN_ERR "%s:%d m2m_desc=%p\n", __func__, __LINE__, dma_m2m_desc);

	dev_printk(KERN_ERR "%s:%d device_prep_dma_memcpy=%p\n", __func__, __LINE__, dma_m2m_chan->device->device_prep_dma_memcpy);
	dev_printk(KERN_ERR "%s:%d device_prep_dma_sg=%p\n", __func__, __LINE__, dma_m2m_chan->device->device_prep_dma_sg);
	
	do_gettimeofday(&end_time);
	start = end_time.tv_sec*1000000 + end_time.tv_usec;


	dma_m2m_desc->callback = dma_m2m_callback;
	dev_printk(KERN_ERR "%s:%d submitting for dma\n", __func__, __LINE__);
	dmaengine_submit(dma_m2m_desc);

	dev_printk(KERN_ERR "%s:%d waiting for completion\n", __func__, __LINE__);
	wait_for_completion(&dma_m2m_ok);

	do_gettimeofday(&end_time);
	end = end_time.tv_sec*1000000 + end_time.tv_usec;
	printk("end - start = %ld us\n", end - start);

	dma_unmap_sg(NULL, sg, DEF_SG_QTY, dma_m2m_config.direction);
	dma_unmap_sg(NULL, sg2, DEF_SG_QTY, dma_m2m_config.direction);


	dev_printk(KERN_ERR "%s:%d count=%d\n", __func__, __LINE__, count);
	return count;
}

struct file_operations dma_fops = {
	open:		sdma_open,
	release:	sdma_release,
	read:		sdma_read,
	write:		sdma_write,
};

int __init sdma_init_module(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	struct device *temp_class;
#else
	struct class_device *temp_class;
#endif
	int error;

	/* register a character device */
	error = register_chrdev(0, "sdma_test", &dma_fops);
	if (error < 0) {
		dev_printk("SDMA test driver can't get major number\n");
		return error;
	}
	gMajor = error;
	dev_printk("SDMA test major number = %d\n",gMajor);

	dma_tm_class = class_create(THIS_MODULE, "sdma_test");
	if (IS_ERR(dma_tm_class)) {
		dev_printk(KERN_ERR "Error creating sdma test module class.\n");
		unregister_chrdev(gMajor, "sdma_test");
		return PTR_ERR(dma_tm_class);
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	temp_class = device_create(dma_tm_class, NULL,
				   MKDEV(gMajor, 0), NULL, "sdma_test");
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	temp_class = device_create(dma_tm_class, NULL,
				   MKDEV(gMajor, 0), "sdma_test");
#else
	temp_class = class_device_create(dma_tm_class, NULL,
					     MKDEV(gMajor, 0), NULL,
					     "sdma_test");
#endif
	if (IS_ERR(temp_class)) {
		dev_printk(KERN_ERR "Error creating sdma test class device.\n");
		class_destroy(dma_tm_class);
		unregister_chrdev(gMajor, "sdma_test");
		return -1;
	}

	dev_printk("SDMA test Driver Module loaded\n");
	return 0;
}

static void sdma_cleanup_module(void)
{
	unregister_chrdev(gMajor, "sdma_test");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	device_destroy(dma_tm_class, MKDEV(gMajor, 0));
#else
	class_device_destroy(dma_tm_class, MKDEV(gMajor, 0));
#endif
	class_destroy(dma_tm_class);

	dev_printk("SDMA test Driver Module Unloaded\n");
}


module_init(sdma_init_module);
module_exit(sdma_cleanup_module);

MODULE_AUTHOR("Freescale Semiconductor");
MODULE_DESCRIPTION("SDMA test driver");
MODULE_LICENSE("GPL");
