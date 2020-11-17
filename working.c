#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <stdlib.h>
#include<time.h>
//#include "fcgi_stdio.h"

static void logging(const char *fmt, ...);

void delay(unsigned int mseconds)
{
    clock_t goal = mseconds + clock();
    while (goal > clock());
}

int main(int argc, char* argv[])
{
	AVOutputFormat *ofmt = NULL;
	//输入对应一个AVFormatContext，输出对应一个AVFormatContext
	//（Input AVFormatContext and Output AVFormatContext）
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	
	//in_filename  = "cuc_ieschool.mov";
	//in_filename  = "cuc_ieschool.mkv";
	//in_filename  = "cuc_ieschool.ts";
	//in_filename  = "cuc_ieschool.mp4";
	//in_filename  = "abc.mp4";
	in_filename  = "virtual_cycling.mp4";//输入URL（Input file URL）
	//in_filename  = "shanghai03_p.h264";
	
	out_filename = "rtmp://localhost/live/bbc";//输出 URL（Output URL）[RTMP]
//	out_filename = "rtsp://localhost:8554/virtual";//输出 URL（Output URL）[UDP]
//	out_filename = "http://localhost:8080/hls/bbb/mpegts"; //[HLS]
//	out_filename = "virtual_cycling_out.mp4";

	av_register_all();
	//Network
	avformat_network_init();
	//输入（Input）
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		printf( "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto end;
	}

	int videoindex=-1;
	int audioindex=-1;
	for(i=0; i<ifmt_ctx->nb_streams; i++) 
		if(ifmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			
		} else if(ifmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
			audioindex=i;
		}

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	//输出（Output）
	
	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename); //RTMP
	//avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtsp", out_filename);//RTSP
	//avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//HLS

	if (!ofmt_ctx) {
		printf( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//根据输入流创建输出流（Create output AVStream according to input AVStream）
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			printf( "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		//复制AVCodecContext的设置（Copy the settings of AVCodecContext）
		ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		if (ret < 0) {
			printf( "Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codecpar->codec_tag = 0;
		/*if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codecpar->flags |= CODEC_FLAG_GLOBAL_HEADER;*/
	}
	//Dump Format------------------
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	//打开输出URL（Open output URL）
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf( "Could not open output URL '%s'", out_filename);
			goto end;
		}
	}
	//写文件头（Write file header）
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf( "Error occurred when opening output URL\n");
		goto end;
	}

	int frame_index=0;
	float count=2.80f;
	int64_t PTS_old = 0; int64_t PTS_new = 0;
	int64_t DTS_old = 0; int64_t DTS_new = 0;
	int64_t gap=0;

	int64_t start_time=av_gettime();
	while (1) {
		AVStream *in_stream, *out_stream;
		//获取一个AVPacket（Get an AVPacket）
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;
		
		//FIX：No PTS (Example: Raw H.264)
		//Simple Write PTS
		if(pkt.pts==AV_NOPTS_VALUE){
			//Write PTS
			AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
			//Parameters
			//av_q2d converts rational to double.
			pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts=pkt.pts;
			pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			logging("AV_NOPTS_VALUE");
		}
		//Important:Delay
		if(pkt.stream_index==videoindex){
			AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
			AVRational time_base_q={1,AV_TIME_BASE};
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
				av_usleep(pts_time - now_time);
		}

		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		//out_stream->time_base = in_stream->time_base;
		/* copy packet */
		//转换PTS/DTS（Convert PTS/DTS）
		/*if(frame_index%500==0 && count>0.70){
			count=count-0.05;
			logging("count=%f",count);
		}*/
		if(pkt.stream_index==videoindex){
		  switch(frame_index)
		  {
		   case 0:
		    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		    PTS_old = pkt.pts;
		    logging("frame_index: %d PTS_old = %d pkt.pts = %d",frame_index,PTS_old,pkt.pts);
		    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		    DTS_old = pkt.dts;
		    logging("frame_index: %d DTS_old = %d pkt.dts = %d",frame_index,DTS_old,pkt.dts);
		    break;
		   case 1:
		    gap = (pkt.dts-DTS_old); 
		    PTS_new = (PTS_old+(count*gap));
		    DTS_new = (DTS_old+(count*gap));
		    logging("gap = %d Updated_pts = %d PTS_New = %d",gap,(PTS_old+(int64_t)(count*gap)),PTS_new);
//		    pkt.pts = av_rescale_q_rnd(PTS_old+(2*gap), in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		    pkt.pts = PTS_new;
//		    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		    pkt.dts = DTS_new;
		    PTS_old = pkt.pts; DTS_old = pkt.dts;
		    logging("PTS_old = %d DTS_old = %d count = %f",PTS_old,DTS_old,(float)count);
		    break;
		   default:
		    PTS_new = (PTS_old+(count*gap));
		    DTS_new = (DTS_old+(count*gap));
		    count=count-0.0005;
//		    pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX)); 
		    pkt.pts = PTS_new;
		    logging("PTS_new = %d PTS_old = %d",PTS_new,PTS_old);
		    PTS_old = pkt.pts;
//		    pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		    pkt.dts = DTS_new;
		    logging("DTS_new = %d DTS_old = %d count = %f",DTS_new,DTS_old,count);
		    DTS_old = pkt.dts;
		  } 
		}else if(pkt.stream_index==audioindex){
		  //  pkt.pts = PTS_old;
		  // pkt.dts = DTS_old;
		   pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		   pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		}
		//pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;

		//Print to Screen
		if(pkt.stream_index==videoindex){
			printf("Send %8d video frames to output URL\n",frame_index);
			logging("AVPacket->pts %d AVPkt -> dts %d Duration %d",pkt.pts,pkt.dts,pkt.duration);
			frame_index++;
		}
		//ret = av_write_frame(ofmt_ctx, &pkt);
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

		if (ret < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		
		av_packet_unref(&pkt);
		delay(50); 
		
	}
	//写文件尾（Write file trailer）
	av_write_trailer(ofmt_ctx);
end:
	avformat_close_input(&ifmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
	return 0;
}

static void logging(const char *fmt, ...){
    va_list args;
    fprintf(stderr, "LOG: ");
    va_start(args,fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
    fprintf(stderr, "\n");
}

