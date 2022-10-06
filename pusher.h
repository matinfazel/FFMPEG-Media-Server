#pragma once

#include <iostream>
#include <thread>
#include <chrono>
#include <assert.h>
#include <list>
#include <mutex>
#include <cstring>
#include <csignal>
#include <cstdlib>
#ifdef __cplusplus 
extern "C" {

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include <libswscale/swscale.h>
#include "libavutil/imgutils.h"

}
using namespace std;
#endif

class FfmpegOutputer {
    enum State {INIT=0, SERVICE, DOWN};
    AVFormatContext *m_ofmt_ctx {nullptr};
    std::string m_url;

    std::thread *m_thread_output {nullptr};
    bool m_thread_output_is_running {false};

    State m_output_state;
    std::mutex m_list_packets_lock;
    std::list<AVPacket*> m_list_packets;
    bool m_repeat {true};

    bool string_start_with(const std::string& s, const std::string& prefix);

    int output_initialize();

    void output_service();

    void output_down();

    void output_process_thread_proc();

public:
    FfmpegOutputer();
    ~FfmpegOutputer();

    int OpenOutputStream(const std::string& url, AVFormatContext *ifmt_ctx);

    int InputPacket(AVPacket *pkt);

    int CloseOutputStream();
};