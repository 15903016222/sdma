#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/delay.h>

#include <linux/dmaengine.h>
#include <linux/device.h>
#include <linux/platform_data/dma-imx.h>
#include <linux/platform_data/dma-imx-sdma.h>

#include <linux/io.h>
#include <linux/delay.h>

static int gMajor; //major number of device
static struct class *dma_tm_class;
static char *wbuf;
static char *rbuf;
static dma_addr_t wpaddr;
static dma_addr_t rpaddr;

struct dma_chan *dma_m2m_chan;

struct completion dma_m2m_ok;

#define SDMA_BUF_SIZE  1024

static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;
	chan->private = param;
	return true;
}

int sdma_open(struct inode * inode, struct file * filp)
{
	dma_cap_mask_t dma_m2m_mask;
	struct imx_dma_data m2m_dma_data = {0};


	init_completion(&dma_m2m_ok);	


	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_SLAVE, dma_m2m_mask);
	m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
	m2m_dma_data.priority = DMA_PRIO_HIGH;
	
	dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!dma_m2m_chan) {
		printk("Error opening the SDMA memory to memory channel\n");
		return -EINVAL;
	}


	wbuf = dma_alloc_coherent(NULL, SDMA_BUF_SIZE, &wpaddr, GFP_DMA);
	rbuf = dma_alloc_coherent(NULL, SDMA_BUF_SIZE, &rpaddr, GFP_DMA);


	return 0;
}

int sdma_release(struct inode * inode, struct file * filp)
{
	dma_release_channel(dma_m2m_chan);
	dma_m2m_chan = NULL;
	dma_free_coherent(NULL, SDMA_BUF_SIZE, wbuf, wpaddr);
	dma_free_coherent(NULL, SDMA_BUF_SIZE, rbuf, rpaddr);


	return 0;
}

ssize_t sdma_read (struct file *filp, char __user * buf, size_t count, loff_t * offset)
{
	int i;
	
	wait_for_completion(&dma_m2m_ok);
	for (i=0; i<10; i++) {
	printk("src_data_%d = %x\n",i, *(wbuf+i) );
	}
	printk ("----------------------------------- \n");
	for (i=0; i<10; i++) {
	printk("dst_data_%d = %x\n",i, *(rbuf+i) );
	}
	printk ("----------------------------------- \n");
	
	return 0;
}

static void dma_m2m_callback(void *data)
{
	printk("in %s\n",__func__);
	complete(&dma_m2m_ok);
	return ;
}

ssize_t sdma_write(struct file * filp, const char __user * buf, size_t count, loff_t * offset)
{
	u32 *index1;
	struct dma_slave_config dma_m2m_config;
	struct dma_async_tx_descriptor *dma_m2m_desc;
	int i;
	index1 = wbuf;

	for (i=0; i<SDMA_BUF_SIZE; i++) {
		*(index1 + i) = 0x12345678;
	}

	for (i=0; i<SDMA_BUF_SIZE; i++) {
		printk("%d : %x\n",i, *(wbuf+i) );
	}

	dma_m2m_config.direction = DMA_MEM_TO_MEM;
	dma_m2m_config.dst_addr = rpaddr;
	dma_m2m_config.src_addr = wpaddr;
	dma_m2m_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	dma_m2m_config.dst_maxburst = 4;
	dma_m2m_config.src_maxburst = 4;
	dmaengine_slave_config(dma_m2m_chan, &dma_m2m_config);

	dma_m2m_desc = dma_m2m_chan->device->device_prep_dma_cyclic(
					dma_m2m_chan, 0, SDMA_BUF_SIZE, SDMA_BUF_SIZE/2, DMA_MEM_TO_MEM, 0);
	dma_m2m_desc->callback = dma_m2m_callback;
	dmaengine_submit(dma_m2m_desc);
	dma_async_issue_pending (dma_m2m_chan);

	return 0;
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
		printk("SDMA test driver can't get major number\n");
		return error;
	}
	gMajor = error;
	printk("SDMA test major number = %d\n",gMajor);


	dma_tm_class = class_create(THIS_MODULE, "sdma_test");
	if (IS_ERR(dma_tm_class)) {
		printk(KERN_ERR "Error creating sdma test module class.\n");
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
		printk(KERN_ERR "Error creating sdma test class device.\n");
		class_destroy(dma_tm_class);
		unregister_chrdev(gMajor, "sdma_test");
		return -1;
	}


	printk("SDMA test Driver Module loaded\n");
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


	printk("SDMA test Driver Module Unloaded\n");
}

module_init(sdma_init_module);
module_exit(sdma_cleanup_module);
MODULE_LICENSE("GPL");
