#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"


/*
 * Calculate the distance between two (x, y) points
 * on a graph and return the int as a distance.
 */
int distance(int x1, int y1, int x2, int y2)
{
  double result;
  double x_squared, y_squared;

  x_squared = (x2 - x1)*(x2 - x1);
  y_squared = (y2 - y1)*(y2 - y1);

  result = sqrt( x_squared  + y_squared );

  return (int)result;
}

/*
 * Draw a circle onto a frame around the center of x, y coordinates
 * Note: Modifies the frame data passed in
 */
void draw_circle(AVFrame *frame, int center_x, int center_y, int radius)
{
  int k, j;

  // Iterate over the data in the AVFrame
  for (k = 0; k < frame->height; k++)
  {
    for (j = 0; j < frame->width * 3; j=j+3)
    {
      // Get the x and y values of the image
      int x = j / 3;
      int y = k;
      // Get the distance of the current pixel. If it is within the circle radius,
      // then draw a sweet orange ball
      int distance1 = distance(x, y, center_x, center_y);

      if (distance1 < radius)
      {
        // Calculate percentage of orange color to be added to our red ball based on percentage of
        // Distance / radius.
        double gradient_pct = ( (double)distance1 / (double)radius );

        // Set the Red value
        *(frame->data[0] +k*frame->linesize[0]+ j) = 255;
        // Set the green value
        *(frame->data[0] + k*frame->linesize[0]+ (j+1)) = (int) (double)(gradient_pct * 185);
        // Set the blue value
        *(frame->data[0] + k*frame->linesize[0]+ (j+2)) = 0;     
      }
    }
  }
}

/**
 * Converts the Frame into the specified format
 * Returns an AVFrame 
 */
AVFrame* convert_frame(AVFrame * input_frame, int format)
{
  // Convert our Frame to a specified color schmeme. 
  // Use a second Frame to store this format of the picture
  AVFrame * output_frame;
  
  output_frame = av_frame_alloc();

  // Even though we have allocated a frame, we still need a place to put the raw
  // data when we convert it. avpicture_get_size will give the the size necessary
  // to allocate the space manually
  uint8_t *buffer;
  int num_bytes;

  // Determine required buffer size and allocate buffer
  num_bytes = avpicture_get_size(format, input_frame->width, input_frame->height);
  buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));

  output_frame->format = format;

  // set up the output width, height, format
  output_frame->height = input_frame->height;
  output_frame->width =  input_frame->width;
 
  

  struct SwsContext *sws_ctx = NULL;
  sws_ctx = sws_getContext
            (
              input_frame->width,
              input_frame->height,
              input_frame->format,
              input_frame->width,
              input_frame->height,
              format,
              SWS_BILINEAR,
              NULL,
              NULL,
              NULL
            );

  // Now we use avpicture_fill to associate the frame with the newly allocated buffer
  avpicture_fill( (AVPicture *)output_frame, buffer, format, input_frame->width, input_frame->height);

  // Set up the sws_scale
  sws_scale (
    sws_ctx,
    (uint8_t const * const *)input_frame->data,
    input_frame->linesize,
    0,
    input_frame->height,
    output_frame->data,
    output_frame->linesize
  );

  // Free memory for original picture
  av_free(sws_ctx);

  return output_frame;
}

/*
 * Get's the frame from the picture provided in command line argument.
 * Return an AVFrame
 */
AVFrame* get_background_frame(char *filename)
{
  // Register all the codecs
  av_register_all();

  AVFormatContext * pic_format_context = NULL;

  // Open the file
  int is_valid = avformat_open_input( &pic_format_context, filename, NULL, NULL);

  // Check if the file could be opened
  if (is_valid != 0)
  {
    printf("The file could not be opened\n");
    exit(1);
  }

  // Retrive stream information
  is_valid  = avformat_find_stream_info(pic_format_context, NULL);
  if (is_valid != 0)
  {
    printf("Error retrieving the stream information\n");
    exit(1);
  }

  // Dump information about file onto standard error
  av_dump_format(pic_format_context, 0, filename, 0);

  // Set the steam of AVCodecContext
  AVCodecContext *pic_codec_context;

  
  pic_codec_context = pic_format_context->streams[0]->codec; // Find the stream
  pic_codec_context->pix_fmt = PIX_FMT_YUV420P; // Set the default format of original frame

  // Find the decoder for the picture
  AVCodec * pic_codec;

  pic_codec = avcodec_find_decoder(pic_codec_context->codec_id);
  if (!pic_codec)
  {
    printf("Error finding the decoder for the pic_codec_context\n");
    exit(1);
  }

  // Open the Codec
  is_valid = avcodec_open2(pic_codec_context, pic_codec, NULL);
  if (is_valid != 0)
  {
    printf("Error opening the codec\n");
    exit(1);
  }

  // *** Storing the Data
  AVFrame * pic_frame; // Frame for the original format of the picture

  pic_frame = av_frame_alloc();

  pic_frame->height = pic_codec_context->height;
  pic_frame->width = pic_codec_context->width;

  // *** Reading the Data from the Stream

  // Read through the entire steam by readin in a packet, decoding it into our frame.
  // Once our frame is complete, we will convert it
  int frame_finished;
  AVPacket packet;
  int decode_result;
  int i = 5;

  while(av_read_frame(pic_format_context, &packet) >= 0)
  {
    if(packet.stream_index == 0)
    {
      // Decode the frame in a packet
      decode_result = avcodec_decode_video2(pic_codec_context, pic_frame, &frame_finished, &packet);

      // Convert image to from it's native format to RGB24
      if(frame_finished)
      {
        // Convert the frame to the proper format
        pic_frame = convert_frame(pic_frame, PIX_FMT_RGB24);
      }
    }
    // Free the packet allocated by av_read_frame
    av_free_packet(&packet);
  }

  // Close the codec
  avcodec_close(pic_codec_context);

  // Cloes the picture format
  avformat_close_input(&pic_format_context);

  return pic_frame;

}

/*
 * Output the frame as a .xkcd file
 */
void save_xkcd(AVFrame *input_frame, int frame_number)
{
  FILE *output_file;
  AVCodec *codec;
  AVFrame *frame;
  AVPacket packet;
  AVCodecContext *context = NULL;
  char filename[32];
  int is_set = 0;
  int width;
  int height;

  codec = avcodec_find_encoder(AV_CODEC_ID_XKCD);//find xkcd 
  printf("name = %s\n", codec->name);
  if (!codec)
  {
    fprintf(stderr, "Codec not found\n");
    exit(1);
  }
  context = avcodec_alloc_context3(codec);
  if (!context)
  {
    fprintf(stderr, "Could not allocate video codec context\n");
    exit(1);
  }

  context->pix_fmt = PIX_FMT_RGB24; //trying to hard code format
  context->height = input_frame->height;
  context->width = input_frame->width;
  width = context->width;
  height = context->height;

  // Open the codec
  int result = avcodec_open2(context, codec, NULL);
  if (result < 0)
  {
    fprintf(stderr, "Could not open codec\n");
    exit(1);
  }
  
  sprintf(filename, "frame%03d.xkcd", frame_number);
  printf("%s\n", filename);
  output_file = fopen(filename, "wb");
  if(!output_file)
  {
   fprintf(stderr, "Could not open file\n"); 
  }

  frame = av_frame_alloc();
  if(!frame)
  {
    fprintf(stderr, "could not allocate a frame\n");
    exit(1);
  }


  frame->format = PIX_FMT_RGB24;
  frame->width = context->width;
  frame->height = context->height;
  frame = convert_frame(input_frame, PIX_FMT_RGB24);

  av_init_packet(&packet);

  packet.data = NULL;
  packet.size = 0;

  int ret = avcodec_encode_video2(context, &packet, frame, &is_set);
  if(ret < 0)
  {
    fprintf(stderr, "Error encoding frame\n");
    exit(1);
  }

  if(is_set)
  {
    fwrite(packet.data, 1, packet.size, output_file);
    av_free_packet(&packet);
  }
  avcodec_close(context);
  av_free(context);
  av_free(frame);

}


/*
 *Calculates the amount of change to simulate a bounce
 */
int change_in_y(int center_y){

  double static step = 0;
  int amp = center_y / 4; //amount of change in y value
  double PI = 3.14159265;
  
  int yPos = -1 * sin(step) * amp;
  step = step + 0.06;//rate of change
  double to_mod = (2 * PI);
  step = fmod(step,to_mod);

  return yPos;
}

/*
 * Runs the main program
 */
int main(int argc, char *argv[])
{

  AVFrame * frame_RGB = get_background_frame(argv[1]); 

  // **** Start Manipulateing AVFrameData
  
  // Find center of the AVFrame.
  int center_x = frame_RGB->width / 2;
  int center_y = frame_RGB->height / 2;

  // radius is %15 of the smaller value of picture width and picture height
  int radius = (frame_RGB->width < frame_RGB->height ? frame_RGB->width * 0.1 : frame_RGB->height * 0.1);

  int i;
  for (i = 0; i < 300; i++){
    AVFrame* tempFrame = convert_frame(frame_RGB, PIX_FMT_RGB24);
    int c = change_in_y(center_x);
    draw_circle(tempFrame, center_x, center_y - c, radius);
    save_xkcd(tempFrame, i);
    av_free(tempFrame);
  }

  return 0;
}
