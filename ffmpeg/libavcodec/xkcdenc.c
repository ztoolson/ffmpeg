/*
 * XKCD image format encoder
 * Zach Toolson and Michael Call. cs3505 University of Utah Spring 2014
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

#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "avcodec.h"
#include "bytestream.h"
#include "xkcd.h"
#include "internal.h"


/* Check and store format information
   AVCodecContext is defined in avcodec.h*/
static av_cold int xkcd_encode_init(AVCodecContext *avctx) {
    /* Check the pix format from the AVCodecContext to ensure that it is RGB8.
     * If not in RGB8, the format is not specified. Throw an error */
    if (avctx->pix_fmt == AV_PIX_FMT_RGB8) {
        avctx->bits_per_coded_sample = 8;
    }
    else {
        av_log(avctx, AV_LOG_INFO, "unsupported pixel format\n");
        return AVERROR(EINVAL);
    }

    /* coded_frame is the picture in the bitstream 
     * Cite bmpenc.c
     */
    avctx->coded_frame = av_frame_alloc();
    if (!avctx->coded_frame)
        return AVERROR(ENOMEM);

    return 0;
}

/*
 * Converts the given data and puts it into a databuffer to be decoded 
 */
static int xkcd_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pict, int *got_packet) {
    // Intialize all local variables to be used
    const AVFrame * const p = pict;
    int n_bytes_image, n_bytes_per_row, n_bytes, i, hsize;

    const uint32_t *pal = NULL;
    uint32_t palette256[256];
    int pad_bytes_per_row, pal_entries = 0;

    int bit_count = avctx->bits_per_coded_sample;
    uint8_t *ptr, *buf;
    
    // Set the picture/frame type
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
    avctx->coded_frame->key_frame = 1;

   if(avctx->pix_fmt == AV_PIX_FMT_RGB8) {
      av_assert1(bit_count == 8);
      avpriv_set_systematic_pal2(palette256, avctx->pix_fmt);
      pal = palette256;
    }

   /* If we have built a palette without palette entries set the number of palette entries
      to be entered */
    if (pal && !pal_entries) {
      pal_entries = 1 << bit_count;//puts size of pallete 
    }

    //set row data
    n_bytes_per_row = ((int64_t)avctx->width * (int64_t)bit_count + 7LL) >> 3LL;
    pad_bytes_per_row = (4 - n_bytes_per_row) & 3;

    //calculate total image size in bytes
    n_bytes_image = avctx->height * (n_bytes_per_row + pad_bytes_per_row);

//Set size of file header and info header
#define SIZE_XKCDFILEHEADER 12
#define SIZE_XKCDINFOHEADER 30
    hsize = SIZE_XKCDFILEHEADER + SIZE_XKCDINFOHEADER  + (pal_entries << 2);
    n_bytes = n_bytes_image + hsize; 

    ff_alloc_packet2(avctx, pkt, n_bytes);

    // Set the buffer pointed to where data starts in memory
    buf = pkt->data;

    // Header Information
    bytestream_put_byte(&buf, 'X');                   // XKCDFILEHEADER.Type
    bytestream_put_byte(&buf, 'K');                   // XKCDFILEHEADER.Type
    bytestream_put_byte(&buf, 'C');                   // XKCDFILEHEADER.Type
    bytestream_put_byte(&buf, 'D');                   // XKCDFILEHEADER.Type
    bytestream_put_le32(&buf, n_bytes);               // XKCDFILEHEADER.Size
    bytestream_put_le32(&buf, hsize);                 // XKCDFILEHEADER.OffBits

    // Info Header Information
    bytestream_put_le32(&buf, SIZE_XKCDINFOHEADER);   // XKCDINFOHEADER.Size
    bytestream_put_le32(&buf, avctx->width);          // XKCDINFOHEADER.Width
    bytestream_put_le32(&buf, avctx->height);         // XKCDINFOHEADER.Height
    bytestream_put_le16(&buf, bit_count);             // XKCDINFOHEADER.BitCount
    bytestream_put_le32(&buf, 0);                     // XKCDINFOHEADER.XPelsPerMeter
    bytestream_put_le32(&buf, 0);                     // XKCDINFOHEADER.YPelsPerMeter
    bytestream_put_le32(&buf, 0);                     // XKCDINFOHEADER.ClrUsed
    bytestream_put_le32(&buf, 0);                     // XKCDINFOHEADER.ClrIMportant

    // add palette entries to buffer
    for (i = 0; i < pal_entries; i++)
       bytestream_put_le32(&buf, pal[i] & 0xFFFFFF);

    ptr = p->data[0] + (avctx->height - 1) * p->linesize[0];
    buf = pkt->data + hsize;

    // place picture data in buffer
    for(i = 0; i < avctx->height; i++) {
        memcpy(buf, ptr, n_bytes_per_row);//Copy image into buffer
        buf += n_bytes_per_row;
        memset(buf, 0, pad_bytes_per_row);//add padding
        buf += pad_bytes_per_row;
        ptr -= p->linesize[0]; // ... and go back
    }

    pkt->flags = pkt->flags | AV_PKT_FLAG_KEY;
    *got_packet = 1;

    return 0;
}

/*
 * Closes encoder using av_frame_free()
 */
static av_cold int xkcd_encode_close(AVCodecContext *avctx)
{
    av_frame_free(&avctx->coded_frame);
    return 0;
}

// Defines the xkcd encoder. states name, functions, and accepted formats
AVCodec ff_xkcd_encoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("XKCD (built for cs3505 in U of U image)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,

    // functions used in xkcdenc.c
    .init           = xkcd_encode_init, 
    .encode2        = xkcd_encode_frame,
    .close          = xkcd_encode_close,

    // formats accepted by xkcd
    .pix_fmts       = (const enum AVPixelFormat[]){
                      AV_PIX_FMT_RGB8,
                      AV_PIX_FMT_NONE },
};
