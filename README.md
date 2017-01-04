DMA实现内存到内存的拷贝出现的问题：
有两种方式实现：
一. cyclic模式：
 1. 申请通道：
    // 通道过滤函数，筛选出合适的通道
    static bool dma_m2m_filter(struct dma_chan *chan, void *param)
    {
        if (!imx_dma_is_general_purpose(chan))
            return false;
        chan->private = param;
        return true;
    }
    // DMA通道类型的数据类型
    dma_cap_mask_t dma_m2m_mask;
    // 初始化或零化通道数据类型
    dma_cap_zero(dma_m2m_mask);
    // 设置通道数据类型为 DMA_SLAVE 型， 还有一种 DMA_MEMCPY 的通道数据类型，不过一直申请
    // 不下来，具体申请不下来的原因查不到，在仿照内核中的用法也是申请失败
    dma_cap_set(DMA_SLAVE, dma_m2m_mask);
    // 定义DMA操作的是Memory
    m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
    m2m_dma_data.priority = DMA_PRIO_HIGH;
    // 申请DMA通道，成功返回dma通道，失败返回 NULL 
    // dma_m2m_filter：上边的过滤函数，m2m_dma_data：向过滤函数中传递的参数
    dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
    if (!dma_m2m_chan) {
        printk("Error opening the SDMA memory to memory channel\n");
        return -EINVAL;
    }

  2. 申请两个DMA缓冲区，1个作为src，1个作为dst，然后配置dma_slave_config
    // 此函数是申请两个缓冲区，wbuf/rbuf是虚拟地址，wpaddr/rpaddr是物理地址
    // 这两个缓冲区申请的时候是有系统自动分配支持DMA传输的内存
    wbuf = dma_alloc_coherent(NULL, SDMA_BUF_SIZE, &wpaddr, GFP_DMA);
    rbuf = dma_alloc_coherent(NULL, SDMA_BUF_SIZE, &rpaddr, GFP_DMA);
    // 配置DMA的传输方向
    dma_m2m_config.direction = DMA_MEM_TO_MEM;
    // 配置DMA传输的目标地址，此地址是物理地址
    dma_m2m_config.dst_addr = rpaddr;
    // 配置DMA传输的源地址， 此地址也是物理地址
    dma_m2m_config.src_addr = wpaddr;
    // 配置从源地址进行DMA传输使用的是多少位，1代表8位，2代表16位，3代表24位，4代表32位
    // 如果此变量没有设置，默认是32位传输
    dma_m2m_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
    // 同源地址一样
    dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
    // 配置每次传输上边位数的最大个数
    dma_m2m_config.dst_maxburst = 4;
    dma_m2m_config.src_maxburst = 4;
    // 将DMA配置信息配置到dma中
    dmaengine_slave_config(dma_m2m_chan, &dma_m2m_config);

  3. 从多个描述符中，找到一个适合该通道的描述符并返回，并给返回的描述符添加回调函数
    
    dma_m2m_desc = dma_m2m_chan->device->device_prep_dma_cyclic(dma_m2m_chan, 0, 
                                 SDMA_BUF_SIZE, SDMA_BUF_SIZE/2, DMA_MEM_TO_MEM, 0);
    dma_m2m_desc->callback = dma_m2m_callback;
    
  4. 将准备好的dma进行提交，并开始执行dma传输
    // 提交dma
    dmaengine_submit(dma_m2m_desc);
    // 开始执行dma传输
    dma_async_issue_pending (dma_m2m_chan);

  5. 等待dma传输结束，调用回调函数
    static void dma_m2m_callback(void *data)
    {
        printk("in %s\n",__func__);
        complete(&dma_m2m_ok);
        return ;
    }

 以上每个步骤都可以成功执行，但是却找不到数据的流向

二. sg模式 （分散/聚合模式）
  1. 申请通道
    与cyclic模式完全相同
    // 通道过滤函数，筛选出合适的通道
    static bool dma_m2m_filter(struct dma_chan *chan, void *param)
    {
        if (!imx_dma_is_general_purpose(chan))
            return false;
        chan->private = param;
        return true;
    }
    // DMA通道类型的数据类型
    dma_cap_mask_t dma_m2m_mask;
    // 初始化或零化通道数据类型
    dma_cap_zero(dma_m2m_mask);
    // 设置通道数据类型为 DMA_SLAVE 型， 还有一种 DMA_MEMCPY 的通道数据类型，不过一直申请
    // 不下来，具体申请不下来的原因查不到，在仿照内核中的用法也是申请失败
    dma_cap_set(DMA_SLAVE, dma_m2m_mask);
    // 定义DMA操作的是Memory
    m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
    m2m_dma_data.priority = DMA_PRIO_HIGH;
    // 申请DMA通道，成功返回dma通道，失败返回 NULL 
    // dma_m2m_filter：上边的过滤函数，m2m_dma_data：向过滤函数中传递的参数
    dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
    if (!dma_m2m_chan) {
        printk("Error opening the SDMA memory to memory channel\n");
        return -EINVAL;
    }

  2. 申请两个DMA缓冲区，配置DMA
    // 配置DMA的传输方向
    dma_m2m_config.direction = DMA_MEM_TO_MEM;
    // 配置DMA传输数据的位数
    dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
    // 将DMA的配置信息配置到dma中
    dmaengine_slave_config(dma_m2m_chan, &dma_m2m_config);

  这一步与设备到设备不太一样，如果目标地址和源地址都是memory，那么
  dma_m2m_config.dst_addr 和 dma_m2m_config.src_addr 这两个参数直接就会被忽略。
  
 3. 获取返回描述符，并给描述符添加回调函数，提交dma，开始执行dma传输
    // 初始化一个sg链表，缓冲区的个数是 1
    sg_init_table(sg, 1);
    // 将申请的DMA源缓冲区加载到sg链表中
    sg_set_buf(&sg[0], wbuf, SDMA_BUF_SIZE);
    // 将缓冲区的虚拟地址和物理地址进行映射
    ret = dma_map_sg(NULL, sg, 1, dma_m2m_config.direction);
    // 获取dma的一个返回描述符，注意：第5个参数是标志 1
    dma_m2m_desc = dma_m2m_chan->device->device_prep_slave_sg(dma_m2m_chan,sg, 1, 
                                                    dma_m2m_config.direction, 1, NULL);
    // 给返回描述符添加回调函数
    dma_m2m_desc->callback = dma_m2m_callback;
    // 提交dma
    dmaengine_submit(dma_m2m_desc);
    // 开始执行本次的dma传输
    dma_async_issue_pending(dma_m2m_chan);

  4. 等待dma传输结束，调用回调函数
    wait_for_completion(&dma_m2m_ok);

    // 向描述符中传递的回调函数
    static void dma_m2m_callback(void *data)
    {
        printk ("%s %s %d \n", __FILE__, __func__, __LINE__);
        complete(&dma_m2m_ok);
        return ;
    }

  5. 与步骤3相同，不同点就是本次初始化了另外一个sg链表，把DMA另一个缓冲区给加载到sg链表，
     然后进行物理地址和虚拟地址的映射，还有就是，获取dma返回描述符的时候第5个参数标志设置成 0
    sg_init_table(sg2, 1);
    sg_set_buf(&sg2[0], rbuf, SDMA_BUF_SIZE);
    ret = dma_map_sg(NULL, sg2, 1, dma_m2m_config.direction);

    dma_m2m_desc = dma_m2m_chan->device->device_prep_slave_sg(dma_m2m_chan,sg2, 1,
                                                     dma_m2m_config.direction, 0, NULL);
    dma_m2m_desc->callback = dma_m2m_callback;
    dmaengine_submit(dma_m2m_desc);
    dma_async_issue_pending(dma_m2m_chan);

  6. 等待dma传输结束，调用回调函数，表明这次dma传输结束。

 这种方式也可以执行到回调函数，回调函数也可正常执行完成，也是找不到数据流向，并且这种方式，通常会访问非法内存，导致程序崩溃，具体问题查不到在什么地方，或者访问了哪一片内存。

