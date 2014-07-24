/*
 * XKCD image format decoder
 * Michael Call and Zach Toolson. cs3505 University of Utah Spring 2014
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "bytestream.h"
#include "xkcd.h"
#include "internal.h"

/*
 * Copies information from the buffer into a xkcd picture format
 */
static int xkcd_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt) {
    const uint8_t *buf = avpkt->data; // Address of Data buffer from ffmpeg
    int buf_size       = avpkt->size; //size of buffer
    AVFrame *p         = data; // picture data

    unsigned int fsize, hsize;
    int width, height;
    unsigned int depth;
    unsigned int ihsize;
    int i, n, linesize, ret;
    uint8_t *ptr;
    int dsize;
    const uint8_t *buf0 = buf;

    // Check header size
    if (buf_size < 12) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", buf_size);
        return AVERROR_INVALIDDATA;
    }

    // Check first 4 byes of the header to follow the ffmpeg convention
    if (bytestream_get_byte(&buf) != 'X' ||
        bytestream_get_byte(&buf) != 'K' ||
        bytestream_get_byte(&buf) != 'C' ||
        bytestream_get_byte(&buf) != 'D' ) {
        av_log(avctx, AV_LOG_ERROR, "Bad Char to start header\n");
        return AVERROR_INVALIDDATA;
    }
    
    // Cite bmp.c
    fsize = bytestream_get_le32(&buf); //fsize is the total size of header and picture data
    fsize = buf_size;
    
    // Get the header sizes
    hsize  = bytestream_get_le32(&buf); // full header size
    ihsize = bytestream_get_le32(&buf); // info header size

    width  = bytestream_get_le32(&buf);
    height = bytestream_get_le32(&buf);

    // depth is bit count in encoding
    depth = bytestream_get_le16(&buf);

    // Pass the width and height information to avctx
    avctx->width  = width;
    avctx->height = height;

    // get palette format based on color depth
    avctx->pix_fmt = AV_PIX_FMT_PAL8;

    // Check if no picture data, return
    if (ff_get_buffer(avctx, p, 0) < 0)
        return ff_get_buffer(avctx, p, 0); // Pass along the ff_get_buffer error
    
    // Set the picture type
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;

    // set buffer to bottom of picture
    buf   = buf0 + hsize;
    dsize = buf_size - hsize;

    // Line size in file multiple of 4
    n = ((avctx->width * depth + 31) / 8) & ~3;
    
    // Cite
    ptr      = p->data[0] + (avctx->height - 1) * p->linesize[0];
    linesize = -p->linesize[0];
  
    // Check that we have the right palette
    int colors = 256;

    // Set aside memory for palette
    memset(p->data[1], 0, 1024);

    buf = buf0 + 12 + ihsize; //set buffer to palette location
    
    // Cite
    for (i = 0; i < colors; i++)
        ((uint32_t*)p->data[1])[i] = 0xFFU << 24 | bytestream_get_le32(&buf);

    buf = buf0 + hsize;

    // Copy from the buffer to the image
    for (i = 0; i < avctx->height; i++) {
        memcpy(ptr, buf, n);
        buf += n;
        ptr += linesize;
    }

    *got_frame = 1;

    return buf_size;
}

AVCodec ff_xkcd_decoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("XKCD (built for cs3505 in U of U image"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,

    // Functions used in decode
    .decode         = xkcd_decode_frame,

    .capabilities   = CODEC_CAP_DR1,
};
