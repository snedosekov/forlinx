/*
 * cssp-camera driver
 *
 * Based on Vivi driver
 *
 * Copyright (C) 2012 QuickLogic Corp.
 *
 * Developed for QuickLogic by:
 * Damian Eppel <damian.eppel@teleca.com>
 * Przemek Szewczyk <przemek.szewczyk@teleca.com>
 * Dan Aizenstros <daizenstros@quicklogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <mach/edma.h>
#include <linux/clk.h>
// V4L2 Interface *********************
#include <media/soc_camera.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
//*************************************
#include "cssp_camera.h"
#include <media/mt9t112.h>
#include <media/soc_camera.h>


/*
 * ---------------------------------------------------------------------------
 *  QuickLoigc Camera Interface registers
 * ---------------------------------------------------------------------------
 */

#define REG_MODE		0x00000
#define REG_DATA		0x10000

/* MODE bit shifts */
#define FMT_2X8_EN		BIT(15) /* Enable 2 byte format on CAMIF bus (0 - 10 bit, 1 - 16 bit 2x8) */
#define PCLK_POL		BIT(14) /* PCLK polarity (0 - rising edge, 1 - falling edge */
#define HS_EN			BIT(13) /* High speed bus (0 =< 50 MHz, 1 > 50 MHz) */
#define ENABLE			BIT(12)
#define LDR_EN			BIT(11) /* Large DMA Request Support (0 - 32 bytes, 1 - 128 bytes) */
#define REV			0xFF	/* Chip Revision mask */


static struct cssp_cam_fmt formats[] = {
	{
		.name	= "4:2:2, packed, YUYV",
		.fourcc	= V4L2_PIX_FMT_YUYV,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_YUYV8_2X8,
	},
/*
 * UYVY doesn't work properly. VYUY and YVYU are not tested.
 * So disable the UYVY, VYUY and YVYU modes for now
 */
#if 0
	{
		.name	= "4:2:2, packed, UYVY",
		.fourcc	= V4L2_PIX_FMT_UYVY,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_UYVY8_2X8,
	},
	{
		.name	= "4:2:2, packed, VYUY",
		.fourcc	= V4L2_PIX_FMT_VYUY,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_VYUY8_2X8,
	},
	{
		.name	= "4:2:2, packed, YVYU",
		.fourcc	= V4L2_PIX_FMT_YVYU,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_YVYU8_2X8,
	},
#endif
	{
		.name	= "RGB565 (LE)",
		.fourcc	= V4L2_PIX_FMT_RGB565,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_RGB565_2X8_LE,
	},
	{
		.name	= "RGB555 (LE)",
		.fourcc	= V4L2_PIX_FMT_RGB555,
		.depth	= 16,
		.code	= V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE,
	},
};


/***************************************************************************/


static int configure_gpio(int nr, int val, const char *name)
{
	unsigned long flags = val ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
	int ret;
	if (!gpio_is_valid(nr))
		return 0;
	ret = gpio_request_one(nr, flags, name);
	if (!ret)
		gpio_export(nr, 0);
	return ret;
}

static int reset_cssp(struct cssp_cam_dev *cam)
{
	struct platform_device *pdev = cam->pdev;
	int err;

	cam->reset_pin = ((struct cssp_cam_platform_data *)pdev->dev.platform_data)->gpio_reset_pin;

	err = configure_gpio(cam->reset_pin, 0, "cssp_reset");
	if (err) {
		dev_err(&pdev->dev, "failed to configure cssp reset pin\n");
		return -1;
	}

	mdelay(1);

	gpio_direction_output(cam->reset_pin, 1);

	return err;
}

static int trigger_dma_transfer_to_buf(struct cssp_cam_dev *dev, struct vb2_buffer *vb)
{
	dma_addr_t dma_buf = vb2_dma_contig_plane_dma_addr(vb, 0);

	if (!dma_buf) {
		/* Is this possible? Release the vb2_buffer with an error here, */
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		dev->current_vb = NULL;
		return -ENOMEM;
	}

	dev->dma_tr_params.dst = dma_buf;

	// Enable DMA
	edma_write_slot(dev->dma_ch, &dev->dma_tr_params);

	dev->current_vb = vb;

	// Enable data capture
	dev->mode |= ENABLE;
	writew(dev->mode, dev->reg_base_virt + REG_MODE);

	return 0;
}

static void dequeue_buffer_for_dma(struct cssp_cam_dev *dev)
{
	struct cssp_cam_dmaqueue *dma_q = &dev->vidq;
	unsigned long flags = 0;

	spin_lock_irqsave(&dev->slock, flags);
	if (!list_empty(&dma_q->active)) {
		struct cssp_cam_buffer *buf;

		buf = list_entry(dma_q->active.next, struct cssp_cam_buffer, list);
		list_del(&buf->list);
		spin_unlock_irqrestore(&dev->slock, flags);

		buf->fmt = dev->fmt;

		trigger_dma_transfer_to_buf(dev, &buf->vb);
	} else {
		spin_unlock_irqrestore(&dev->slock, flags);
	}
}

static void dma_callback(unsigned lch, u16 ch_status, void *data)
{
	struct cssp_cam_dev *dev = (struct cssp_cam_dev *)data;

	// Disable data capture
	dev->mode &= ~ENABLE;
	writew(dev->mode, dev->reg_base_virt + REG_MODE);

	if (ch_status == DMA_COMPLETE) {
		struct vb2_buffer *vb = dev->current_vb;
		struct edmacc_param dma_tr_params;

		edma_read_slot(dev->dma_ch, &dma_tr_params);
		if ((dma_tr_params.opt != 0) ||
			(dma_tr_params.src != 0) ||
			(dma_tr_params.a_b_cnt != 0) ||
			(dma_tr_params.dst != 0) ||
			(dma_tr_params.src_dst_bidx != 0) ||
			(dma_tr_params.link_bcntrld != 0xffff) ||
			(dma_tr_params.src_dst_cidx != 0) ||
			(dma_tr_params.ccnt != 0)) {

			trigger_dma_transfer_to_buf(dev, dev->current_vb);
			return;
		}

		vb->v4l2_buf.field = dev->field;
		vb->v4l2_buf.sequence = dev->frame_cnt++;
		do_gettimeofday(&vb->v4l2_buf.timestamp);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		dev->current_vb = NULL;

		/* check if we have new buffer queued */
		dequeue_buffer_for_dma(dev);
	} else {
		/* we got a missed interrupt so just start a new DMA with the existing buffer */
		if (dev->current_vb != NULL) {
			if (trigger_dma_transfer_to_buf(dev, dev->current_vb))
				dev_err(&dev->pdev->dev, "No buffer allocated!\n");
		}
	}
}

static int configure_edma(struct cssp_cam_dev *cam)
{
	struct platform_device *pdev = cam->pdev;
	int dma_channel;

	dma_channel = ((struct cssp_cam_platform_data *)pdev->dev.platform_data)->dma_ch;

	pdev->dev.dma_mask = &cam->dma_mask;

	pdev->dev.coherent_dma_mask = (u32)~0;

	if (dma_set_mask(&pdev->dev, (u32)~0)) {
		dev_err(&pdev->dev, "failed setting mask for DMA\n");
		return -1;
	}

	cam->dma_ch = edma_alloc_channel(dma_channel, dma_callback, cam, EVENTQ_0);
	if (cam->dma_ch < 0) {
		dev_err(&pdev->dev, "allocating channel for DMA failed\n");
		return -EBUSY;
	} else {
		dev_info(&pdev->dev, "allocating channel for DMA succeeded, chan=%d\n", cam->dma_ch);
	}

	cam->dma_req_len = cam->rev > 3 ? 128 : 32;

	cam->dma_tr_params.opt = TCINTEN | TCC(cam->dma_ch);
	cam->dma_tr_params.src = cam->reg_base_phys + REG_DATA;
	cam->dma_tr_params.a_b_cnt = ACNT(cam->dma_req_len) | BCNT((VGA_WIDTH * BYTES_PER_PIXEL) / cam->dma_req_len);
	cam->dma_tr_params.src_dst_bidx = SRCBIDX(0) | DSTBIDX(cam->dma_req_len);
	cam->dma_tr_params.link_bcntrld = BCNTRLD((VGA_WIDTH * BYTES_PER_PIXEL) / cam->dma_req_len) | LINK(0xffff);
	cam->dma_tr_params.src_dst_cidx = SRCCIDX(0) | DSTCIDX(cam->dma_req_len);
	cam->dma_tr_params.ccnt = CCNT(VGA_HEIGHT);

	return 0;
}

static int configure_cssp(struct cssp_cam_dev *cam)
{
	struct platform_device *pdev = cam->pdev;
	int ret = 0;
	unsigned int val;
	struct resource *res;

	ret = reset_cssp(cam);
	if (ret)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpmc_phys_mem_slot");
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get gpmc_phys_mem_slot resource\n");
		return -ENODEV;
	}

	/*
	 * Request the region.
	 */
	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		return -EBUSY;
	}

	cam->reg_base_phys = res->start;
	cam->reg_size = resource_size(res);

	cam->reg_base_virt = (unsigned int)ioremap(cam->reg_base_phys, cam->reg_size);
	if (cam->reg_base_virt == 0) {
		dev_err(&pdev->dev, "ioremap of registers region failed\n");
		release_mem_region(cam->reg_base_phys, cam->reg_size);
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "reg_base_virt = 0x%x\n", cam->reg_base_virt);

	val = readw(cam->reg_base_virt + REG_MODE);
	cam->rev = val & REV;
	dev_info(&pdev->dev, "CSSP Revision %c%d\n", 'A' + ((cam->rev & 0xf0) >> 4), cam->rev & 0x0f);

	return 0;
}

static int configure_camera_sensor(struct cssp_cam_dev *cam)
{
	struct i2c_board_info *info = cam->camera_board_info;
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	struct v4l2_subdev *subdev;
	struct v4l2_mbus_framefmt f_format = {
			.width = VGA_WIDTH,
			.height = VGA_HEIGHT,
			.code = V4L2_MBUS_FMT_YUYV8_2X8,
			.colorspace = V4L2_COLORSPACE_JPEG,
	};

	if (cam->rev > 4) {
		struct soc_camera_link *scl = (struct soc_camera_link *)info->platform_data;
		struct mt9t112_camera_info *mci = (struct mt9t112_camera_info *)scl->priv;

		mci->flags |= MT9T112_FLAG_HISPEED;
	}

	/* Enable the clock just for the time of loading the camera driver and disable after that */
	/* It is going to be be re-enabled later, when camera will be in use */
	clk_enable(cam->camera_clk);
	udelay(5); // let the clock stabilize

	adapter	= i2c_get_adapter(((struct soc_camera_link *)(info->platform_data))->i2c_adapter_id);
	if (!adapter) {
		dev_err(&cam->pdev->dev, "failed to get i2c adapter...\n");
		return -ENODEV;
	}

	client = i2c_new_device(adapter, info);
	i2c_put_adapter(adapter);

	if (client == NULL) {
		return -ENODEV;
	}

	dev_info(&cam->pdev->dev, "client's name is: %s\n", client->name);

	subdev = (struct v4l2_subdev *)i2c_get_clientdata(client);
	if (subdev == NULL) {
		printk(KERN_INFO "[%s]: Error with i2c_get_clientdata", __func__);
		i2c_unregister_device(client);
		return -ENODEV;
	}

	cam->subdev = subdev;

	v4l2_subdev_call(subdev, video, s_mbus_fmt, &f_format);

	clk_disable(cam->camera_clk);

	return 0;
}

static int start_camera_sensor(struct cssp_cam_dev *cam)
{
	clk_enable(cam->camera_clk);
	udelay(5); /* let the clock stabilize */

	v4l2_subdev_call(cam->subdev, video, s_stream, 1);

	return 0;
}

static void stop_camera_sensor(struct cssp_cam_dev *cam)
{
	v4l2_subdev_call(cam->subdev, video, s_stream, 0);

	clk_disable(cam->camera_clk);

	return;
}


/************************************************
 * 				Video4Linux2
 */

static struct cssp_cam_fmt *get_format(struct v4l2_format *f)
{
	struct cssp_cam_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}


/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/

static int queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], void *alloc_ctxs[])
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vq);
	unsigned long size;

	size = dev->sizeimage;

	if (0 == *nbuffers)
		*nbuffers = 32;

	while (size * *nbuffers > vid_limit * 1024 * 1024)
		(*nbuffers)--;

	*nplanes = 1;

	sizes[0] = size;

	alloc_ctxs[0] = dev->dma_cont_ctx;

	dev->frame_cnt = 0;

	dprintk(dev, 1, "%s, count=%d, size=%ld\n", __func__, *nbuffers, size);

	return 0;
}

static int buffer_init(struct vb2_buffer *vb)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vb->vb2_queue);

	BUG_ON(NULL == dev->fmt);

	/*
	 * This callback is called once per buffer, after its allocation.
	 *
	 * Vivi does not allow changing format during streaming, but it is
	 * possible to do so when streaming is paused (i.e. in streamoff state).
	 * Buffers however are not freed when going into streamoff and so
	 * buffer size verification has to be done in buffer_prepare, on each
	 * qbuf.
	 * It would be best to move verification code here to buf_init and
	 * s_fmt though.
	 */

	return 0;
}

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct cssp_cam_buffer *buf = container_of(vb, struct cssp_cam_buffer, vb);
	unsigned long size;

	dprintk(dev, 1, "%s, field=%d\n", __func__, vb->v4l2_buf.field);

	BUG_ON(NULL == dev->fmt);

	/*
	 * Theses properties only change when queue is idle, see s_fmt.
	 * The below checks should not be performed here, on each
	 * buffer_prepare (i.e. on each qbuf). Most of the code in this function
	 * should thus be moved to buffer_init and s_fmt.
	 */
	if (dev->width  < 48 || dev->width  > MAX_WIDTH ||
	    dev->height < 32 || dev->height > MAX_HEIGHT)
		return -EINVAL;

	size = dev->sizeimage;
	if (vb2_plane_size(vb, 0) < size) {
		dprintk(dev, 1, "%s data will not fit into plane (%lu < %lu)\n",
				__func__, vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb, 0, size);

	buf->fmt = dev->fmt;

	return 0;
}

static int buffer_finish(struct vb2_buffer *vb)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	dprintk(dev, 1, "%s\n", __func__);
	return 0;
}

static void buffer_cleanup(struct vb2_buffer *vb)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	dprintk(dev, 1, "%s\n", __func__);
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct cssp_cam_buffer *buf = container_of(vb, struct cssp_cam_buffer, vb);
	struct cssp_cam_dmaqueue *vidq = &dev->vidq;
	unsigned long flags = 0;

	dprintk(dev, 1, "%s\n", __func__);

	if (dev->streaming_started && !dev->current_vb) {
		trigger_dma_transfer_to_buf(dev, &buf->vb);
	} else {
		spin_lock_irqsave(&dev->slock, flags);
		list_add_tail(&buf->list, &vidq->active);
		spin_unlock_irqrestore(&dev->slock, flags);
	}
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vq);
	int ret;

	dprintk(dev, 1, "%s\n", __func__);

	ret = start_camera_sensor(dev);
	if (ret != 0)
		return ret;

	// Enable DMA
	edma_start(dev->dma_ch);

	dev->streaming_started = 1;

	/* check if we have new buffer queued */
	dequeue_buffer_for_dma(dev);

	return 0;
}

/* abort streaming and wait for last buffer */
static int stop_streaming(struct vb2_queue *vq)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vq);
	struct cssp_cam_dmaqueue *dma_q = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);

	// Disable DMA
	edma_stop(dev->dma_ch);

	// Disable data capture
	dev->mode &= ~ENABLE;
	writew(dev->mode, dev->reg_base_virt + REG_MODE);

	stop_camera_sensor(dev);

	dev->streaming_started = 0;

	/* Release all active buffers */
	while (!list_empty(&dma_q->active)) {
		struct cssp_cam_buffer *buf;

		buf = list_entry(dma_q->active.next, struct cssp_cam_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		dprintk(dev, 2, "[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
	}

	dev->current_vb = NULL;

	return 0;
}

static void cssp_cam_lock(struct vb2_queue *vq)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vq);
	mutex_lock(&dev->mutex);
}

static void cssp_cam_unlock(struct vb2_queue *vq)
{
	struct cssp_cam_dev *dev = vb2_get_drv_priv(vq);
	mutex_unlock(&dev->mutex);
}

static struct vb2_ops cssp_cam_video_qops = {
	.queue_setup		= queue_setup,
	.buf_init		= buffer_init,
	.buf_prepare		= buffer_prepare,
	.buf_finish		= buffer_finish,
	.buf_cleanup		= buffer_cleanup,
	.buf_queue		= buffer_queue,
	.start_streaming	= start_streaming,
	.stop_streaming		= stop_streaming,
	.wait_prepare		= cssp_cam_unlock,
	.wait_finish		= cssp_cam_lock,
};


/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/

static int vidioc_querycap(struct file *file, void *priv,
					struct v4l2_capability *cap)
{
	struct cssp_cam_dev *dev = video_drvdata(file);

	strcpy(cap->driver, "cssp_camera");
	strcpy(cap->card, "cssp_camera");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
			    V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_fmtdesc *f)
{
	struct cssp_cam_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct cssp_cam_dev *dev = video_drvdata(file);

	f->fmt.pix.width	= dev->width;
	f->fmt.pix.height	= dev->height;
	f->fmt.pix.field	= dev->field;
	f->fmt.pix.pixelformat	= dev->fmt->fourcc;
	f->fmt.pix.bytesperline	= dev->bytesperline;
	f->fmt.pix.sizeimage	= dev->sizeimage;
	f->fmt.pix.colorspace	= dev->colorspace;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	struct cssp_cam_fmt *fmt;
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	v4l2_fill_mbus_format(&mbus_fmt, pix, fmt->code);
	v4l2_subdev_call(dev->subdev, video, try_mbus_fmt, &mbus_fmt);
	v4l2_fill_pix_format(pix, &mbus_fmt);
	pix->bytesperline = (pix->width * fmt->depth) >> 3;
	pix->sizeimage = pix->height * pix->bytesperline;

	if ((pix->sizeimage % dev->dma_req_len) != 0)
		return -EINVAL;

	switch (mbus_fmt.field) {
	case V4L2_FIELD_ANY:
		pix->field = V4L2_FIELD_NONE;
		break;
	case V4L2_FIELD_NONE:
		break;
	default:
		dev_err(&dev->pdev->dev, "Field type %d unsupported.\n", mbus_fmt.field);
		return -EINVAL;
	}

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	struct vb2_queue *q = &dev->vb_vidq;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mbus_fmt;
	int i = 0, rem;
	u32 bytesperline, height;

	int ret = vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0)
		return ret;

	if (vb2_is_streaming(q)) {
		dprintk(dev, 1, "%s device busy\n", __func__);
		return -EBUSY;
	}

	dev->fmt = get_format(f);
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	dev->field = f->fmt.pix.field;
	dev->colorspace = f->fmt.pix.colorspace;
	dev->bytesperline = f->fmt.pix.bytesperline;
	dev->sizeimage = f->fmt.pix.sizeimage;

	/* Set the sensor into the new format */
	v4l2_fill_mbus_format(&mbus_fmt, pix, dev->fmt->code);
	v4l2_subdev_call(dev->subdev, video, s_mbus_fmt, &mbus_fmt);

	/* Calculate DMA transfer parameters based on DMA request length */
	bytesperline = dev->bytesperline;
	do {
		rem = bytesperline % dev->dma_req_len;
		if (rem != 0) {
			bytesperline <<= 1;
			i++;
		}
	} while (rem != 0);
	height = dev->height >> i;

	/* Set the EDMA for the new resolution */
	dev->dma_tr_params.a_b_cnt = ACNT(dev->dma_req_len) | BCNT(bytesperline / dev->dma_req_len);
	dev->dma_tr_params.link_bcntrld = BCNTRLD(bytesperline / dev->dma_req_len) | LINK(0xffff);
	dev->dma_tr_params.ccnt = CCNT(height);

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_reqbufs(&dev->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_querybuf(&dev->vb_vidq, p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_qbuf(&dev->vb_vidq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_dqbuf(&dev->vb_vidq, p, file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_streamon(&dev->vb_vidq, i);
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	return vb2_streamoff(&dev->vb_vidq, i);
}

static int vidioc_log_status(struct file *file, void *priv)
{
	struct cssp_cam_dev *dev = video_drvdata(file);

	v4l2_ctrl_handler_log_status(&dev->ctrl_handler, dev->v4l2_dev.name);
	return 0;
}

static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index > 0)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	sprintf(inp->name, "Camera %u", inp->index);

	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	if (i > 0)
		return -EINVAL;

	return 0;
}

static int vidioc_subscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_event_subscribe(fh, sub, 0);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ioctl_ops cssp_cam_ioctl_ops = {
	.vidioc_querycap		= vidioc_querycap,
	.vidioc_enum_fmt_vid_cap	= vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs			= vidioc_reqbufs,
	.vidioc_querybuf		= vidioc_querybuf,
	.vidioc_qbuf			= vidioc_qbuf,
	.vidioc_dqbuf			= vidioc_dqbuf,
	.vidioc_enum_input		= vidioc_enum_input,
	.vidioc_g_input			= vidioc_g_input,
	.vidioc_s_input			= vidioc_s_input,
	.vidioc_streamon		= vidioc_streamon,
	.vidioc_streamoff		= vidioc_streamoff,
	.vidioc_log_status		= vidioc_log_status,
	.vidioc_subscribe_event		= vidioc_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};


/* ------------------------------------------------------------------
	File operations
   ------------------------------------------------------------------*/

static unsigned int video_poll(struct file *file, struct poll_table_struct *wait)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	struct v4l2_fh *fh = file->private_data;
	struct vb2_queue *q = &dev->vb_vidq;
	unsigned int res;

	dprintk(dev, 1, "%s\n", __func__);
	res = vb2_poll(q, file, wait);
	if (v4l2_event_pending(fh))
		res |= POLLPRI;
	else
		poll_wait(file, &fh->wait, wait);
	return res;
}

static int video_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct cssp_cam_dev *dev = video_drvdata(file);
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = vb2_mmap(&dev->vb_vidq, vma);
	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start,
		ret);
	return ret;
}

static ssize_t video_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	struct cssp_cam_dev *cam_dev = video_drvdata(file);

	dprintk(cam_dev, 1, "read called\n");
	return vb2_read(&cam_dev->vb_vidq, buf, size, offset, file->f_flags & O_NONBLOCK);
}

static int video_close(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct cssp_cam_dev *cam_dev = video_drvdata(file);

	dprintk(cam_dev, 1, "close called (dev=%s), file %p\n",
		video_device_node_name(vdev), file);

	if (v4l2_fh_is_singular_file(file))
		vb2_queue_release(&cam_dev->vb_vidq);
	return v4l2_fh_release(file);
}

static const struct v4l2_file_operations cssp_cam_fops = {
	.owner		= THIS_MODULE,
	.open		= v4l2_fh_open,
	.release	= video_close,
	.read		= video_read,
	.poll		= video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= video_mmap,
};


/* ------------------------------------------------------------------
	Driver initialization
   ------------------------------------------------------------------*/

static struct video_device cssp_cam_template = {
	.name		= "cssp_camera",
	.fops		= &cssp_cam_fops,
	.ioctl_ops	= &cssp_cam_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release,
};

static int __init  video_probe(struct cssp_cam_dev *cam_dev)
{
	struct video_device *vfd;
	struct v4l2_ctrl_handler *hdl;
	struct vb2_queue *q;
	int ret = 0;

	snprintf(cam_dev->v4l2_dev.name, sizeof(cam_dev->v4l2_dev.name),
			"%s-%03d", "cssp_camera", 0);
	ret = v4l2_device_register(NULL, &cam_dev->v4l2_dev);
	if (ret)
		goto free_dev;

	cam_dev->fmt = &formats[0];
	cam_dev->width = VGA_WIDTH;
	cam_dev->height = VGA_HEIGHT;
	cam_dev->sizeimage = VGA_WIDTH * VGA_HEIGHT * BYTES_PER_PIXEL;
	hdl = &cam_dev->ctrl_handler;
	v4l2_ctrl_handler_init(hdl, 0);

	if (hdl->error) {
		ret = hdl->error;
		goto unreg_dev;
	}
	cam_dev->v4l2_dev.ctrl_handler = hdl;

	/* initialize locks */
	spin_lock_init(&cam_dev->slock);

	/* initialize queue */
	q = &cam_dev->vb_vidq;
	memset(q, 0, sizeof(cam_dev->vb_vidq));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_READ;
	q->drv_priv = cam_dev;
	q->buf_struct_size = sizeof(struct cssp_cam_buffer);
	q->ops = &cssp_cam_video_qops;
	q->mem_ops = &vb2_dma_contig_memops;

	vb2_queue_init(q);

	mutex_init(&cam_dev->mutex);

	/* init video dma queues */
	INIT_LIST_HEAD(&cam_dev->vidq.active);

	ret = -ENOMEM;
	vfd = video_device_alloc();
	if (!vfd)
		goto unreg_dev;

	*vfd = cssp_cam_template;
	vfd->debug = debug;
	vfd->v4l2_dev = &cam_dev->v4l2_dev;
	set_bit(V4L2_FL_USE_FH_PRIO, &vfd->flags);

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &cam_dev->mutex;

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
	if (ret < 0)
		goto rel_vdev;

	video_set_drvdata(vfd, cam_dev);

	if (video_nr != -1)
		video_nr++;

	cam_dev->vdev = vfd;
	v4l2_info(&cam_dev->v4l2_dev, "V4L2 device registered as %s\n",
	video_device_node_name(vfd));

	return 0;

rel_vdev:
	video_device_release(vfd);
unreg_dev:
	v4l2_ctrl_handler_free(hdl);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
free_dev:
	return ret;
}

static int video_remove(struct cssp_cam_dev *cam_dev)
{
	if (cam_dev->dma_cont_ctx != NULL)
		vb2_dma_contig_cleanup_ctx(cam_dev->dma_cont_ctx);

	v4l2_info(&cam_dev->v4l2_dev, "unregistering %s\n",
			video_device_node_name(cam_dev->vdev));
	video_unregister_device(cam_dev->vdev);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
	v4l2_ctrl_handler_free(&cam_dev->ctrl_handler);

	return 0;
}

static int __init  cssp_cam_probe(struct platform_device *pdev)
{
	struct cssp_cam_dev *cam_dev;
	int ret = 0;
	struct cssp_cam_platform_data *cssp_cam_platform_data;

	cssp_cam_platform_data = (struct cssp_cam_platform_data *) pdev->dev.platform_data;
	if (cssp_cam_platform_data == NULL) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODEV;
	}

	if (cssp_cam_platform_data->cam_i2c_board_info == NULL) {
		dev_err(&pdev->dev, "missing camera i2c board info\n");
		return -ENODEV;
	}

	cam_dev = kzalloc(sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	cam_dev->pdev = pdev;
	platform_set_drvdata(pdev, cam_dev);

	cam_dev->camera_board_info = cssp_cam_platform_data->cam_i2c_board_info;

	cam_dev->camera_clk = clk_get(&pdev->dev, cssp_cam_platform_data->cam_clk_name);
	if (IS_ERR(cam_dev->camera_clk)) {
		ret = PTR_ERR(cam_dev->camera_clk);
		dev_err(&pdev->dev, "cannot clk_get %s\n", cssp_cam_platform_data->cam_clk_name);
		goto fail0;
	}

	ret = configure_cssp(cam_dev);
	if (ret)
		goto fail1;

	ret = configure_edma(cam_dev);
	if (ret)
		goto fail2;

	cam_dev->mode = FMT_2X8_EN | PCLK_POL | HS_EN;	// falling edge
	if (cam_dev->rev > 3)
		cam_dev->mode |= LDR_EN;

	ret = configure_camera_sensor(cam_dev);
	if (ret) {
		dev_err(&pdev->dev, "camera sensor configuration failed\n");
		goto fail3;
	}

	cam_dev->dma_cont_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(cam_dev->dma_cont_ctx)) {
		ret = PTR_ERR(cam_dev->dma_cont_ctx);
		goto fail3;
	}

	ret = video_probe(cam_dev);
	if (ret)
		goto fail4;

	return ret;

fail4:
	vb2_dma_contig_cleanup_ctx(cam_dev->dma_cont_ctx);

fail3:
	edma_free_channel(cam_dev->dma_ch);

fail2:
	gpio_free(cam_dev->reset_pin);
	iounmap((void *)cam_dev->reg_base_virt);
	release_mem_region(cam_dev->reg_base_phys, cam_dev->reg_size);

fail1:
	clk_put(cam_dev->camera_clk);

fail0:
	kfree(cam_dev);

	return ret;
}

static int cssp_cam_remove(struct platform_device *pdev)
{
	struct cssp_cam_dev *cam = platform_get_drvdata(pdev);

	iounmap((void *)cam->reg_base_virt);

	release_mem_region(cam->reg_base_phys, cam->reg_size);

	gpio_free(cam->reset_pin);

	edma_free_channel(cam->dma_ch);

	video_remove(cam);

	clk_put(cam->camera_clk);

	kfree(cam);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}


static struct platform_driver cssp_cam_driver = {
	.probe		= cssp_cam_probe,
	.remove		= __devexit_p(cssp_cam_remove),
	.driver		= {
		.name	= "cssp-camera",
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(cssp_cam_driver);


/*
 * Macros sets license, author and description
 */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dan Aizenstros, Damian Eppel, Przemek Szewczyk");
MODULE_DESCRIPTION("QuickLogic Camera Interface driver");

