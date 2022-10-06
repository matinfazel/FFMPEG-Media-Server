#include "pusher.h"
bool interupt = false;
FILE* dst_file = fopen("camera.mp4","wb");

void encode(AVCodecContext *enc_ctx, AVFrame *frame, FfmpegOutputer * pusher);
void decode(AVCodecContext *pCodecContext,AVCodecContext * pCodecencode, AVFrame *pFrame, AVFrame *Frame_tmp,AVPacket *pPacket,FfmpegOutputer * pusher);
void convert_to_rgb(AVFrame * pFrame, AVFrame * Frame_tmp);
void delete_parameters(AVFormatContext * ifmt_ctx,  FfmpegOutputer * pusher);
void logging(const char *fmt, ...);
void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);
void signalHandler( int signum );

int main(int argc, char *argv[])
{
    AVFormatContext *ifmt_ctx = NULL;
    char *in_filename, *out_filename;
    FfmpegOutputer *pusher = NULL;
    int ret;
    signal(SIGINT, signalHandler); 

    int video_stream_index = -1;

    in_filename = argv[1];
    out_filename = argv[2];

    const AVCodec *pCodec = NULL;
    AVCodec *CodecEncode = NULL;
    AVCodecParameters *pCodecParameters =  NULL;
    AVCodecParameters *PCodecEncodeParameters = NULL;
    
    avformat_network_init();

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        delete_parameters(ifmt_ctx,pusher);
    }
   
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        delete_parameters(ifmt_ctx,pusher);
    }
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++){

        AVCodecParameters *pLocalCodecParameters = ifmt_ctx->streams[i]->codecpar;
        
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {

            if (video_stream_index == -1) {
                video_stream_index = i;
                pCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);
                CodecEncode = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
                pCodecParameters = ifmt_ctx->streams[i]->codecpar;
                PCodecEncodeParameters = ifmt_ctx->streams[i]->codecpar;
            }
                
            logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
        } 
        else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
        logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
        }

        logging("\tCodec %s ID %d", pCodec->name, pCodec->id);
    }

    if (video_stream_index == -1) {
        logging("File does not contain a video stream!");
        return -1;
    }

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    AVCodecContext *pCodecencode = avcodec_alloc_context3(CodecEncode);
    pCodecencode->pix_fmt = AV_PIX_FMT_RGB24;
    pCodecencode->time_base.num = ifmt_ctx->streams[video_stream_index]->time_base.num;  
    pCodecencode->time_base.den = ifmt_ctx->streams[video_stream_index]->time_base.den;
    

    if (!pCodecContext){
        logging("failed to allocated memory for pCodecContext");
        return -1;
    }

    if (!pCodecencode){
        logging("failed to allocated memory for pCodecencode");
        return -1;
    }

    
    if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0)
    {
        logging("failed to copy codec params to codec pCodecContext");
        return -1;
    }

    PCodecEncodeParameters->codec_id = pCodecencode->codec_id;
    if (avcodec_parameters_to_context(pCodecencode, PCodecEncodeParameters) < 0)
    {
        logging("failed to copy codec params to codec pCodecencode");
        return -1;
    }
    
    if (avcodec_open2(pCodecContext, pCodec, NULL) < 0)
    {
        logging("failed to open codec through pCodecContext");
        return -1;
    }

    if (avcodec_open2(pCodecencode, CodecEncode, NULL)){
        logging("failed to open codec through pCodecencode");
        return -1;
    }


    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket)
    {
        logging("failed to allocate memory for AVPacket");
        return -1;
    }
    
    AVFormatContext * out_context = NULL;
    if (NULL == pusher) {
        pusher = new FfmpegOutputer();
        int ret = pusher->OpenOutputStream(out_filename, ifmt_ctx);
        if (ret != 0){
            std::cout<<"ERRor in open output file "<<out_filename<<std::endl;
        delete_parameters(ifmt_ctx,pusher);
        }
    }
   
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    
    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame)
    {
        logging("failed to allocate memory for pFrame");
        return -1;
    }

    av_image_alloc(pFrame->data, pFrame->linesize,
        pCodecParameters->width, pCodecParameters->height,static_cast<AVPixelFormat>(pCodecParameters->format), 1);

    AVFrame *Frame_tmp = av_frame_alloc();
    av_image_alloc(Frame_tmp->data, Frame_tmp->linesize,
             pCodecParameters->width, pCodecParameters->height,AV_PIX_FMT_RGB24, 1);
   
    int frame_index = 0;
    while (true) {
        if (interupt == false){

            if (av_read_frame(ifmt_ctx, pPacket) < 0)
                 break;
            decode(pCodecContext,pCodecencode, pFrame,Frame_tmp, pPacket,pusher);
        }
        if (++frame_index == 150)
              exit(1);
    }
    
    for (int i=0;i<8;i++)
        av_freep(&pFrame->data[i]);
    av_frame_free(&pFrame);

    delete_parameters(ifmt_ctx,pusher);
    return 0;
}


void encode(AVCodecContext *enc_ctx, AVFrame *frame, FfmpegOutputer * pusher)
{
    int ret;
    int index = 0;
    AVPacket *pkt;
    pkt = av_packet_alloc();

    if (!pkt){
            std::cout<<"Cannot allocate packet for PKT"<<std::endl;
            exit(1);
        }
    
    if (frame)
        printf("Send frame %3"PRId64"\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0) {
        
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }
        
        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
                
        fwrite(pkt->data, 1, pkt->size, dst_file);
        pusher->InputPacket(pkt);
        av_packet_unref(pkt);

    }
    
}

void decode(AVCodecContext *pCodecContext,AVCodecContext * pCodecencode, AVFrame *pFrame,AVFrame *Frame_tmp, AVPacket *pPacket,FfmpegOutputer * pusher){
                
        int frame_index = 0;
        if (pPacket->pts == AV_NOPTS_VALUE) {
            pPacket->dts = pPacket->pts = (1.0/30)*90*frame_index;
        }

        int response = avcodec_send_packet(pCodecContext, pPacket);
        if (response < 0){
            std::cout<<"Error while sending a packet to the decoder:"<<std::endl;
        }
            
        while(response >= 0 ){
                
            response = avcodec_receive_frame(pCodecContext, pFrame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF){    
                break;   
            }
            else if (response < 0){
                std::cout<<"Error while receiving a frame from the decoder: "<<std::endl;                   
                return;
            }

            logging(
                "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
                pCodecContext->frame_number,
                av_get_picture_type_char(pFrame->pict_type),
                pFrame->pkt_size,
                pFrame->format,
                pFrame->pts,
                pFrame->key_frame,
                pFrame->coded_picture_number
            );
            
            Frame_tmp->height = pFrame->height;
            Frame_tmp->width = pFrame->width;
            Frame_tmp->format = AV_PIX_FMT_RGB24;
            Frame_tmp->time_base = pFrame->time_base;
            Frame_tmp->pts = pFrame->pts;
            Frame_tmp->key_frame = pFrame->key_frame;
            Frame_tmp->sample_rate = pFrame->sample_rate;
            Frame_tmp->sample_aspect_ratio = pFrame->sample_aspect_ratio;
            Frame_tmp->repeat_pict = pFrame->repeat_pict;
            
            convert_to_rgb(pFrame,Frame_tmp);
            
            encode(pCodecencode,Frame_tmp,pusher);
        }

}
void convert_to_rgb(AVFrame * pFrame, AVFrame * Frame_tmp){

    SwsContext *sws_ctx  = sws_getContext(pFrame->width,
    pFrame->height,static_cast<AVPixelFormat>(pFrame->format),
    pFrame->width,pFrame->height,AV_PIX_FMT_RGB24,
    SWS_BILINEAR,NULL,NULL,NULL);

    sws_scale(sws_ctx, (const uint8_t * const*)pFrame->data,
            pFrame->linesize, 0, pFrame->height, Frame_tmp->data, Frame_tmp->linesize);
    
    sws_freeContext(sws_ctx);
    
}


void delete_parameters(AVFormatContext * ifmt_ctx,  FfmpegOutputer * pusher) {
    
    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ifmt_ctx);
    delete pusher;
    exit(0);
}

void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
void signalHandler( int signum ) {
   
   if (interupt == false ){
        std::cout << "************Video is Paused!**********"<<std::endl;
        interupt = true;
   }
   else{ 
        std::cout << "************Video is Play!**********"<<std::endl;
        interupt = false;

    }  
}
