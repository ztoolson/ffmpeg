all:	bouncer

bouncer: bouncer.c
	gcc -I../ffmpeg/include -L../ffmpeg/lib -o bouncer  bouncer.c `pkg-config --cflags --libs libavutil libavformat libavcodec libavfilter libswscale`

clean:	
	rm -f *.o *.xkcd *.mp4

movie:
	./bouncer test.jpg
	ffmpeg -f image2 -i frame%03d.xkcd movie.mp4
