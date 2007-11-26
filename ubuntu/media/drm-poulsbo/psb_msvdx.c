/**
 * file psb_msvdx.c
 * MSVDX I/O operations and IRQ handling
 *
 */

/**************************************************************************
 *
 * Copyright (c) 2007 Intel Corporation, Hillsboro, OR, USA
 * Copyright (c) Imagination Technologies Limited, UK
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "drmP.h"
#include "drm_os_linux.h"
#include "psb_drv.h"
#include "psb_drm.h"
#include "psb_msvdx.h"

#include <asm/io.h>
#include <linux/delay.h>

int psb_submit_video_cmdbuf(struct drm_device * dev,
		     struct drm_buffer_object * cmd_buffer,
		     unsigned long cmd_offset, unsigned long cmd_size, struct drm_fence_object *fence)
{
	int ret;
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned long cmd_page_offset = cmd_offset & ~PAGE_MASK;
	struct drm_bo_kmap_obj cmd_kmap;
	void *cmd;
	uint32_t sequence = fence->sequence;	
	int is_iomem;

	psb_schedule_watchdog(dev_priv);

	/* command buffers may not exceed page boundary */
	if (cmd_size + cmd_page_offset > PAGE_SIZE)
		return -EINVAL;

	ret = drm_bo_kmap(cmd_buffer, cmd_offset >> PAGE_SHIFT,
			  2, &cmd_kmap);
	PSB_DEBUG_GENERAL("MSVDX\n");
	if (ret)
	{
		PSB_DEBUG_GENERAL("MSVDX:ret:%d\n", ret);
		return ret;
	}
	cmd = (void *) drm_bmo_virtual(&cmd_kmap, &is_iomem);
	cmd += cmd_page_offset;


	while(cmd_size > 0)
	{
		uint32_t cur_cmd_size = MEMIO_READ_FIELD(cmd, FWRK_GENMSG_SIZE);
		uint32_t cur_cmd_id = MEMIO_READ_FIELD(cmd, FWRK_GENMSG_ID);
		PSB_DEBUG_GENERAL("cmd start at %08x cur_cmd_size = %d cur_cmd_id = %02x\n", (uint32_t) cmd, 
			cur_cmd_size, cur_cmd_id);
		if ((cur_cmd_size % sizeof(uint32_t)) ||
		    (cur_cmd_size > cmd_size))
		{
			ret = -EINVAL;
			PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);
			goto out;
		}

		switch(cur_cmd_id)
		{
		  case VA_MSGID_RENDER:
		  		/* Fence ID */
		  		MEMIO_WRITE_FIELD(cmd, FW_VA_RENDER_FENCE_VALUE, sequence);
		  		
				/* PTD */
				MEMIO_WRITE_FIELD(cmd, FW_VA_RENDER_MMUPTD, psb_get_default_pd_addr(dev_priv->mmu));

				break;

		  default:
		  		/* Msg not supported */
				ret = -EINVAL;
				PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);
				goto out;
		}
		
		if (dev_priv->msvdx_needs_reset)
		{
			PSB_DEBUG_GENERAL("MSVDX: Needs reset\n");
			if (psb_msvdx_reset(dev_priv))
			{
				ret = -EBUSY;
				PSB_DEBUG_GENERAL("MSVDX: Reset failed\n");
				goto out;
			}
			PSB_DEBUG_GENERAL("MSVDX: Reset ok\n");
			dev_priv->msvdx_needs_reset = 0;
			
			psb_msvdx_init(dev);
			PSB_DEBUG_GENERAL("MSVDX: Init ok\n");
		}

		/* Send the message to h/w */ 
		psb_mtx_send(dev_priv, cmd);
		cmd += cur_cmd_size;
		cmd_size -= cur_cmd_size;
	}

out:
	PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);

	drm_bo_kunmap(&cmd_kmap);

	return ret;
}

/***********************************************************************************
 * Function Name      : psb_mtx_send
 * Inputs             : 
 * Outputs            : 
 * Returns            : 
 * Description        : 
 ************************************************************************************/
void psb_mtx_send(struct drm_psb_private *dev_priv, const void *pvMsg)
{

	static uint32_t padMessage[FWRK_PADMSG_SIZE];
	
	const uint32_t * pui32Msg = (uint32_t *) pvMsg;
	uint32_t		msgNumWords, wordsFree, readIndex, writeIndex;

	PSB_DEBUG_GENERAL("MSVDX: psb_mtx_send\n");	

	msgNumWords = (MEMIO_READ_FIELD(pvMsg, FWRK_GENMSG_SIZE) + 3) / 4;

	PSB_ASSERT(msgNumWords <= NUM_WORDS_MTX_BUF);

	readIndex =	PSB_RMSVDX32( MSVDX_COMMS_TO_MTX_RD_INDEX );
	writeIndex=	PSB_RMSVDX32( MSVDX_COMMS_TO_MTX_WRT_INDEX );

	if (writeIndex + msgNumWords > NUM_WORDS_MTX_BUF) 
	{ /* message would wrap, need to send a pad message*/
		PSB_ASSERT(MEMIO_READ_FIELD(pvMsg, FWRK_GENMSG_ID) != FWRK_MSGID_PADDING);
		/* if the read pointer is at zero then we must wait for it to change otherwise the write
		 pointer will equal the read pointer,which should only happen when the buffer is empty
		 */
		if (0 == readIndex)
		{
/* Todo : This maybe should be a poll */
			for(;;)
			{
				readIndex = PSB_RMSVDX32( MSVDX_COMMS_TO_MTX_RD_INDEX );
				if( readIndex > 0 )
				{
					break;
				}
				DRM_UDELAY(500);
			}
		}
		MEMIO_WRITE_FIELD(padMessage, FWRK_GENMSG_SIZE, (NUM_WORDS_MTX_BUF - writeIndex) << 2);
		MEMIO_WRITE_FIELD(padMessage, FWRK_GENMSG_ID, FWRK_MSGID_PADDING);
		psb_mtx_send(dev_priv, padMessage);
	}

	writeIndex = PSB_RMSVDX32( MSVDX_COMMS_TO_MTX_WRT_INDEX );
	for(;;)
	{
/* Todo : This maybe should be a poll or a special case */
		readIndex = PSB_RMSVDX32( MSVDX_COMMS_TO_MTX_RD_INDEX );

		wordsFree = writeIndex >= readIndex ? NUM_WORDS_MTX_BUF - (writeIndex - readIndex) : readIndex - writeIndex;

		if( wordsFree > msgNumWords )
		{
			break;
		}

		DRM_UDELAY(500);
	}

	while (msgNumWords > 0) 
	{
		PSB_WMSVDX32( *pui32Msg++, MSVDX_COMMS_TO_MTX_BUF + (writeIndex << 2));
		msgNumWords--;
		writeIndex++;
		if (NUM_WORDS_MTX_BUF == writeIndex)
		{
			writeIndex = 0;
		}
	}
	PSB_WMSVDX32( writeIndex, MSVDX_COMMS_TO_MTX_WRT_INDEX );

	/* signal an interrupt to let the mtx know there is a new message */
	PSB_WMSVDX32( 1, MSVDX_MTX_KICKI );
}

/*
 * MSVDX MTX interrupt
 */
void psb_msvdx_mtx_interrupt(struct drm_device *dev)
{
	static uint32_t msgBuffer[128];
	uint32_t	readIndex, writeIndex;
	uint32_t	msgNumWords, msgWordOffset;
 	int	bBatch = 1;
	
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	PSB_DEBUG_GENERAL("Got an MSVDX MTX interrupt\n");

	do
	{
		readIndex = 	PSB_RMSVDX32( MSVDX_COMMS_TO_HOST_RD_INDEX );
		writeIndex = 	PSB_RMSVDX32( MSVDX_COMMS_TO_HOST_WRT_INDEX );

		if( readIndex != writeIndex )
		{
			msgWordOffset = 0;

			msgBuffer[msgWordOffset] = PSB_RMSVDX32( MSVDX_COMMS_TO_HOST_BUF + (readIndex << 2) );

			msgNumWords = (MEMIO_READ_FIELD(msgBuffer, FWRK_GENMSG_SIZE) + 3) / 4;	/* round to nearest word */

			/*ASSERT(msgNumWords <= sizeof(msgBuffer) / sizeof(uint32_t));*/

			if (++readIndex >= NUM_WORDS_HOST_BUF) readIndex = 0;

			for (msgWordOffset++; msgWordOffset < msgNumWords; msgWordOffset++) 
			{
				msgBuffer[msgWordOffset] = PSB_RMSVDX32( MSVDX_COMMS_TO_HOST_BUF + (readIndex << 2) );
				
				if (++readIndex >= NUM_WORDS_HOST_BUF)
				{
					readIndex = 0;
				}
			}

			/* Update the Read index */
			PSB_WMSVDX32( readIndex, MSVDX_COMMS_TO_HOST_RD_INDEX );

			switch (MEMIO_READ_FIELD(msgBuffer, FWRK_GENMSG_ID))
			{
				case VA_MSGID_CMD_FAILED:
				{
					uint32_t ui32Fence		= MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_FAILED_FENCE_VALUE);
					uint32_t ui32FaultStatus	= MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_FAILED_IRQSTATUS);

					PSB_DEBUG_GENERAL("MSVDX: VA_MSGID_CMD_FAILED: Msvdx fault detected - Fence: %08x, Status: %08x - resetting and ignoring error\n", 
						ui32Fence, ui32FaultStatus);

						
					dev_priv->msvdx_needs_reset = 1;
					if (ui32Fence)
					{
						dev_priv->msvdx_current_sequence = ui32Fence;
					}
					else
					{
						if (dev_priv->msvdx_current_sequence < dev_priv->sequence[PSB_ENGINE_VIDEO])
							dev_priv->msvdx_current_sequence++;
						PSB_DEBUG_GENERAL("MSVDX: Fence ID missing, assuming %08x\n", dev_priv->msvdx_current_sequence);
					}

					psb_fence_error(dev,PSB_ENGINE_VIDEO, dev_priv->msvdx_current_sequence, 
						DRM_FENCE_TYPE_EXE, DRM_CMD_FAILED);

					break;
				}
				case VA_MSGID_CMD_COMPLETED:
				{
					uint32_t ui32Fence = MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_COMPLETED_FENCE_VALUE);
					uint32_t ui32TickCount = MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_COMPLETED_NO_TICKS);

					bBatch = 0;					/* This compleated  so return */

					PSB_DEBUG_GENERAL("msvdx VA_MSGID_CMD_COMPLETED: FenceID: %08x, TickCount: %08x\n", ui32Fence, ui32TickCount);
					dev_priv->msvdx_current_sequence = ui32Fence;
					
					psb_fence_handler(dev, PSB_ENGINE_VIDEO);

					break;
				}				
				case VA_MSGID_CMD_COMPLETED_BATCH:
				{
					uint32_t ui32Fence = MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_COMPLETED_FENCE_VALUE);
					uint32_t ui32TickCount = MEMIO_READ_FIELD(msgBuffer, FW_VA_CMD_COMPLETED_NO_TICKS);

					/* we have the fence value in the message */

					PSB_DEBUG_GENERAL("msvdx VA_MSGID_CMD_COMPLETED_BATCH: FenceID: %08x, TickCount: %08x\n", ui32Fence, ui32TickCount);
					dev_priv->msvdx_current_sequence = ui32Fence;
					
					psb_fence_handler(dev, PSB_ENGINE_VIDEO);

					break;
				}
				case VA_MSGID_ACK:
					PSB_DEBUG_GENERAL("msvdx VA_MSGID_ACK\n");
					break;
					
				case VA_MSGID_TEST1 :
					PSB_DEBUG_GENERAL("msvdx VA_MSGID_TEST1\n");
					break;
					
				case VA_MSGID_TEST2 :					
					PSB_DEBUG_GENERAL("msvdx VA_MSGID_TEST2\n");
					break;
					/* Don't need to do anything with these messages */

				case VA_MSGID_DEBLOCK_REQUIRED:
				{
					uint32_t ui32ContextId = MEMIO_READ_FIELD(msgBuffer, FW_VA_DEBLOCK_REQUIRED_CONTEXT);

					/* The BE we now be locked. */

					/* Unblock rendec by reading the mtx2mtx end of slice */
					(void) PSB_RMSVDX32( MSVDX_RENDEC_READ_DATA );

					PSB_DEBUG_GENERAL("msvdx VA_MSGID_DEBLOCK_REQUIRED Context=%08x\n", ui32ContextId);

					break;
				}

				default:
					{
						PSB_DEBUG_GENERAL("ERROR: msvdx Unknown message from MTX \n");
					}
					break;

			}
			/*TBD: psISRInfo->bInterruptProcessed = IMG_TRUE;*/
		}
		else
		{
			/* Get out of here if nothing */
			break;
		}
	}while(bBatch ); /* If this was a batch, we must consume to the end one which casued the int */

	DRM_MEMORYBARRIER();/*TBD check this...*/
}
