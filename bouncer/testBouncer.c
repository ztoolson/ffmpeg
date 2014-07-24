/*
 * Authors: Zach Toolson
 *          Michael call
 *
 * March 18, 2014
 *
 */

#include <stdio.h>
#include <string.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"

/*
 * Verify's that the input file is a .jpg
 * Return 0 if it is valid, otherwise returns -1
 */
int verify_filename (char* file)
{
    return 0;
}

/*
 * Write the AVFrame to an output file that is a ppm. This is for debuggin purposes
 * cite: dranger ffmpeg tutorial
 */
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++){
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    
  }

  // Close file
  fclose(pFile);
}



/*
 * Opens up the filename given, and returns an AVFrame in the RGB24 format
 */
AVFrame* open_file(char* filename)
{
    // Important variables used to get frame
    AVFormatContext *pFormatCtx;

    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    AVFrame *pFrame; // Frame with original format
    AVFrame *pFrameRGB; // Frame with RGB format

    av_register_all();

    // Initiaze memory for Format context struct
    pFormatCtx = avformat_alloc_context();

    //open an input stream and read the header
    int is_open = avformat_open_input(&pFormatCtx, filename, NULL, NULL);
    if(is_open != 0)
      return NULL; //could not open file

    // Updates the pFormatCtx variables including width and height
    int found_stream = avformat_find_stream_info(pFormatCtx, NULL);
    if(found_stream != 0)
      return NULL; //could not find stream 

    // Dump information about file into standard error
    av_dump_format(pFormatCtx, 0, filename, 0);

    pCodecCtx = pFormatCtx->streams[0]->codec;//find the stream
    pCodecCtx->pix_fmt = PIX_FMT_YUV420P; // Set the default format of original frame

    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    // OPen the codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) <0)
    {
      printf("avcodec_open failed\n");
      return NULL;
    }

    // Allocate space for original frame and a RGB24 converted frame
    pFrame = av_frame_alloc();
    pFrameRGB= av_frame_alloc();

    // Calculate the number of bytes for the new RGB24 formatted frame
    int numBytes = avpicture_get_size( PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    // Using the buffer, transfer data into the pFrameRGB in RGB24 format
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

    printf("Height: %i\n", pFrameRGB->height);
    printf("Width: %i\n", pFrameRGB->height);
    // *** Read the data from the AVFrame
    int frameFinished;
    AVPacket packet;


    struct SwsContext *resize;
    resize = sws_getContext(pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

//    int i = 0;
//    int frame_number = 0;
//    // Is this a packet from our picture?
//    while(av_read_frame(pFormatCtx, &packet) >= 0)
//    {
//      if(packet.stream_index == 0)
//      {
//        avcodec_decode_video2(pCodecCtx, pFrame, &frame_number, &packet);
//
//        if(frame_number){
//          //Convert the image to RGB
//          sws_scale(resize, (uint8_t const * const *)pFrame->data, pFrame->linesize,0, pCodecCtx->height, pFrameRGB->data,pFrameRGB->linesize);
//         
//          // @TODO Will remove this eventually
//          //save frame to disk
//          if(++i<=5)
//          {
//            SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i );
//          }
//        }
//      }
//    }
    return pFrameRGB;
}


/*
 * Main Function
 */
int main(int argc, char *argv[])
{
  // Register all possible codecs that we might use
  av_register_all();

  // *****Check command line arguments 
  if(argc != 2)
  {
    printf("Wrong number of arguments\n");
    return;
  }
  // Verify correct filename. In this case only .jpg are valid
  int valid_file  = verify_filename(argv[1]);
  if (valid_file != 0)
  {
    return -1;
  }


  // ***** Open the file and convert into an AVFrame with RGB24 formatting with
  AVFrame * frame = open_file(argv[1]);

  printf("Height: %i\n", frame->height);
  printf("Width: %i\n", frame->height);
  
  // **** Iterate over all the pixels in the AVFrame
  int i;
  int j;

  // Iterate one pixel at a time
  for (i = 0; i < frame->height; i++)
  {
    for (j = 0; j < frame->width*3; j++)
    {
      printf("Hit pixel");
      printf("%x", *(frame->data[0] + i*frame->linesize[0] + j));
    }
  }
//  FILE *fp;
//  int c;
//  int count = 0;
//  fp = fopen("frame1.ppm","r");
//  while(1)
//    {
//      c = fgetc(fp);
//      if(feof(fp) ){
//        break;
//      }
//      if(count % 3 == 0)
//      {
//      printf("\n");
//      }
//     printf("%i ", c);
//     count++;
//    }
//  fclose(fp);
//      

    
 // int i ;
 // for (i = 0; i < 100; i++)
 // {
 //   printf("\nthis is the data \n %i",*(frame->data[0] + i*frame->linesize[0]));
 // }
  // ****** Read data from the AVFrame
//  AVPacket packet;
//
//  int check_packet = av_read_frame(pFormatCtx, &packet);
//  printf("this is the packet int %i", check_packet);
//
//  int packet_stream = packet.stream_index;
//
//  printf("this is the packet stream %i", packet_stream);



}
