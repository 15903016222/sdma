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

#define DMA_DATA_ADDR           0x30000000
#define DMA_DATA_LENGTH         0x00010000

#define BUFF_DATA_ADDR          0x40000000
#define BUFF_DATA_LENGTH        0x00010000

u32 *data_addr;
u32 *buff_addr;

#define SDMA_BUF_SIZE  1024

u32 *wbuf;
u32 *rbuf;

struct dma_chan *dma_m2m_chan_rx;
struct dma_chan *dma_m2m_chan_tx;

struct completion dma_m2m_ok;

struct scatterlist sg[1], sg2[1];

static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;
	chan->private = param;
	return true;
}

static void dma_m2m_callback_rx(void *data)
{
	u32 i;
	printk("in %s\n",__func__);
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(wbuf + %d) = 0x%x \n", i, *(wbuf + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(rbuf + %d) = 0x%x \n", i, *(rbuf + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(data_addr + %d) = 0x%x \n", i, *(data_addr + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(buff_addr + %d) = 0x%x \n", i, *(buff_addr + i));
	}
	printk ("-------------------------------------------------------- \n");
	complete(&dma_m2m_ok);
	return ;
}

static void dma_m2m_callback_tx(void *data)
{
	u32 i;
	printk("in %s\n",__func__);
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(wbuf + %d) = 0x%x \n", i, *(wbuf + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(rbuf + %d) = 0x%x \n", i, *(rbuf + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(data_addr + %d) = 0x%x \n", i, *(data_addr + i));
	}
	printk ("-------------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(buff_addr + %d) = 0x%x \n", i, *(buff_addr + i));
	}
	printk ("-------------------------------------------------------- \n");
	complete(&dma_m2m_ok);
	return ;
}

static int sdma_test(void)
{
	u32 ret;
	dma_cap_mask_t dma_m2m_mask;
	struct dma_slave_config dma_m2m_config;
	struct dma_async_tx_descriptor *dma_m2m_desc_rx;
	struct dma_async_tx_descriptor *dma_m2m_desc_tx;
	struct imx_dma_data m2m_dma_data = {0};

	init_completion(&dma_m2m_ok);

	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_SLAVE, dma_m2m_mask);
	m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
	m2m_dma_data.priority = DMA_PRIO_HIGH;

	dma_m2m_chan_rx = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!dma_m2m_chan_rx) {
		printk("Error opening the SDMA memory to memory channel\n");
		return -EINVAL;
	}

	dma_m2m_config.direction = DMA_DEV_TO_MEM;
	dma_m2m_config.src_addr = DMA_DATA_ADDR;
	dma_m2m_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	dma_m2m_config.src_maxburst = 64;
	ret = dmaengine_slave_config(dma_m2m_chan_rx, &dma_m2m_config);
	if (ret < 0) {
		printk("dmaengine slave config failed\n");
		return ret;
	}

	dma_m2m_chan_tx = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!dma_m2m_chan_tx) {
		printk("Error opening the SDMA memory to memory channel\n");
		return -EINVAL;
	}

	dma_m2m_config.direction = DMA_MEM_TO_DEV;
	dma_m2m_config.dst_addr = BUFF_DATA_ADDR;
	dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	dma_m2m_config.dst_maxburst = 64;
	ret = dmaengine_slave_config(dma_m2m_chan_tx, &dma_m2m_config);
	if (ret < 0) {
		printk("dmaengine slave config failed\n");
		return ret;
	}

/* --------------------------------------------------------------------- */
	sg_init_table(sg2, 1);
	printk("finish sg_init_table\n");

	sg_set_buf(sg2, wbuf, SDMA_BUF_SIZE);
	printk("finish sg_set_buf\n");

	ret = dma_map_sg(NULL, sg2, 1, DMA_FROM_DEVICE);
	if (ret < 0) {
		printk("DMA mapping error.\n");
		return ret;
	}

	dma_m2m_desc_tx = dma_m2m_chan_tx->device->device_prep_slave_sg(dma_m2m_chan_tx, sg2, 1, dma_m2m_config.direction, 0, NULL);

	dma_m2m_desc_tx->callback = dma_m2m_callback_tx;
	dma_m2m_desc_tx->tx_submit (dma_m2m_desc_tx);
	dma_m2m_chan_tx->device->device_issue_pending(dma_m2m_chan_tx);
/* --------------------------------------------------------------------- */

	wait_for_completion(&dma_m2m_ok);
	init_completion(&dma_m2m_ok);

/* ---------------------------------------------------------------------- */	
	sg_init_table(sg, 1);
	printk("finish sg_init_table\n");

	sg_set_buf(sg, rbuf, SDMA_BUF_SIZE);
	printk("finish sg_set_buf\n");

	ret = dma_map_sg(NULL, sg, 1, DMA_TO_DEVICE);
	if (ret < 0) {
		printk("DMA mapping error.\n");
		return ret;
	}

	dma_m2m_desc_rx = dma_m2m_chan_rx->device->device_prep_slave_sg(dma_m2m_chan_rx, sg, 1, dma_m2m_config.direction, 1, NULL);

	dma_m2m_desc_rx->callback = dma_m2m_callback_rx;
	dma_m2m_desc_rx->tx_submit (dma_m2m_desc_rx);
	dma_m2m_chan_rx->device->device_issue_pending(dma_m2m_chan_rx);
/* --------------------------------------------------------------------- */

	wait_for_completion(&dma_m2m_ok);

	return 0;
}

int __init sdma_init_module(void)
{
	u32 i;
	init_completion(&dma_m2m_ok);

	printk("SDMA test Driver Module loaded \n");
    request_mem_region(DMA_DATA_ADDR, DMA_DATA_LENGTH, "dma_data");
    data_addr = (u32 *)ioremap(DMA_DATA_ADDR, DMA_DATA_LENGTH);
    memset((void*)data_addr , 'A' , DMA_DATA_LENGTH) ;
    printk("ioremap buff_data succeed\n");

    request_mem_region(BUFF_DATA_ADDR, BUFF_DATA_LENGTH, "buff_data");
    buff_addr = (u32 *)ioremap(BUFF_DATA_ADDR, BUFF_DATA_LENGTH);
    memset((void*)buff_addr , 'B' , BUFF_DATA_LENGTH) ;
	printk("ioremap dma_data succeed\n");

	wbuf = kmalloc(SDMA_BUF_SIZE, GFP_DMA);
	if(!wbuf) {
		printk("error wbuf !!!!!!!!!!!\n");
		return -1;
	}
	memset (wbuf, 0x12, SDMA_BUF_SIZE);
	printk("kmalloc wbuf succeed\n");

	rbuf = kmalloc(SDMA_BUF_SIZE, GFP_DMA);
	if(!rbuf) {
		printk("error rbuf !!!!!!!!!!!\n");
		return -1;
	}
	memset (rbuf, 0x34, SDMA_BUF_SIZE);
	printk("kmalloc rbuf succeed\n");

	for (i = 0; i < 10; ++i) {
		printk ("*(wbuf + %d) = 0x%x \n", i, *(wbuf + i));
	}
	printk ("---------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(rbuf + %d) = 0x%x \n", i, *(rbuf + i));
	}
	printk ("---------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(data_addr + %d) = 0x%x \n", i, *(data_addr + i));
	}
	printk ("---------------------------------------------------- \n");
	for (i = 0; i < 10; ++i) {
		printk ("*(buff_addr + %d) = 0x%x \n", i, *(buff_addr + i));
	}
	printk ("---------------------------------------------------- \n");

	sdma_test ();
	
	return 0;
}

static void sdma_cleanup_module(void)
{
	printk("SDMA test Driver Module Unloaded\n");
}

module_init(sdma_init_module);
module_exit(sdma_cleanup_module);
MODULE_LICENSE("GPL");

