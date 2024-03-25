#include "radiodelay.h"

#ifdef __linux__
#include "radiodelay.xpm"
#endif

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
// #define MA_NO_PULSEAUDIO
#include "miniaudio.h"
#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLE_RATE 48000
#define ABS(a) ((a) < 0 ? -(a) : a)
#define TIMER 0.01

static struct {
    bool play;
    int driver;
    int in;
    int out;
    float delay;
    const char *skipfile;
} opts = {.play = false, .driver = 0, .in = 0, .out = 0, .delay = 1.00, .skipfile = ""};

static ma_context context;
static ma_timer timer;
static bool recording;
static bool playing;
static ma_device ma_device_in;
static ma_device ma_device_out;
static ma_decoder ma_skip_decoder;
static ma_device ma_device_skip;
static ma_pcm_rb ma_pcm_rb_in;
static float amplitude_in[2];
static float amplitude_out[2];
static ma_backend enabledBackends[MA_BACKEND_COUNT];

Fl_Choice *choice_driver = (Fl_Choice *)0;
Fl_Choice *choice_input = (Fl_Choice *)0;
Fl_Progress *progress_left_in = (Fl_Progress *)0;
Fl_Progress *progress_right_in = (Fl_Progress *)0;
Fl_Slider *slider_input_delay = (Fl_Slider *)0;
Fl_Value_Input *input_delay = (Fl_Value_Input *)0;
Fl_Progress *progress_input_delay = (Fl_Progress *)0;
Fl_Choice *choice_output = (Fl_Choice *)0;
Fl_Progress *progress_left_out = (Fl_Progress *)0;
Fl_Progress *progress_right_out = (Fl_Progress *)0;
Fl_Light_Button *button_play = (Fl_Light_Button *)0;
Fl_Light_Button *button_skip = (Fl_Light_Button *)0;
Fl_File_Input *input_skipfile = (Fl_File_Input *)0;
Fl_Window *window_about = (Fl_Window *)0;
Fl_Double_Window *window_main = (Fl_Double_Window *)0;
Fl_Button *button_mixer = (Fl_Button *)0;
Fl_Button *button_exit = (Fl_Button *)0;

const char *get_opt(const char *name, int argc, char **argv, int i) {
    char *a = argv[i];
    if (strcmp(a, name) == 0) {
        int next = i + 1;
        if (argc > next && argv[next][0] != '-') {
            return argv[next];
        } else {
            return "1";
        }
    }
    return NULL;
}

int arg_handler(int argc, char **argv, int &i) {
    const char *play = get_opt("-play", argc, argv, i);
    if (play) {
        opts.play = true;
        i += 1;
        return 1;
    }
    const char *delay = get_opt("-delay", argc, argv, i);
    if (delay) {
        opts.delay = atof(delay);
        i += 2;
        return 2;
    }
    const char *driver = get_opt("-driver", argc, argv, i);
    if (driver) {
        opts.driver = atoi(driver);
        i += 2;
        return 2;
    }
    const char *in = get_opt("-in", argc, argv, i);
    if (in) {
        opts.in = atoi(in);
        i += 2;
        return 2;
    }
    const char *out = get_opt("-out", argc, argv, i);
    if (out) {
        opts.out = atoi(out);
        i += 2;
        return 2;
    }
    const char *skipfile = get_opt("-skipfile", argc, argv, i);
    if (skipfile) {
        opts.skipfile = skipfile;
        i += 2;
        return 2;
    }
    return 0;
}

static void cb_mixer(Fl_Widget *, void *userdata) {
#ifdef __APPLE__
    char file[] = "file:///System/Library/PreferencePanes/Sound.prefPane";
#elif WIN32
    char file[] = "file://C:/Windows/System32/SndVol.exe";
#elif __linux__
    char file[] = "/usr/bin/pavucontrol";
#endif
#ifdef __linux__
    if (system(file) == -1) {
        fl_alert(file);
    }
#else
    char errmsg[512];
    if (!fl_open_uri(file, errmsg, sizeof(errmsg))) {
        fl_alert(errmsg);
    }
#endif
}

static void cb_exit(Fl_Widget *, void *userdata) {
    exit(0);
}

static void cb_playback(ma_device *pDevice, void *pOutput, const void *pInput,
                        ma_uint32 frameCount) {
    ma_uint32 framesToRead = frameCount;
    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pDevice->playback.format,
                              pDevice->playback.channels);
    void *pReadBuffer;
    ma_result result =
        ma_pcm_rb_acquire_read(&ma_pcm_rb_in, &framesToRead, &pReadBuffer);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to acquire read.\n");
        return;
    }
    memcpy(pOutput, pReadBuffer, framesToRead * bytesPerFrame);
    result = ma_pcm_rb_commit_read(&ma_pcm_rb_in, framesToRead);
    if (result != MA_SUCCESS) {
        // fl_alert("Failed to commit read.\n");
        return;
    }
    ma_uint32 frame = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float left = ABS(((float *)pOutput)[frame]);
        float right = ABS(((float *)pOutput)[frame + 1]);
        if (left > amplitude_out[0]) {
            amplitude_out[0] = left;
        }
        if (right > amplitude_out[1]) {
            amplitude_out[1] = right;
        }
        frame += 2;
    }
}

static void cb_capture(ma_device *pDevice, void *pOutput, const void *pInput,
                       ma_uint32 frameCount) {
    ma_uint32 framesToWrite = frameCount;
    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pDevice->capture.format,
                              pDevice->capture.channels);
    void *pWriteBuffer;
    ma_result result =
        ma_pcm_rb_acquire_write(&ma_pcm_rb_in, &framesToWrite, &pWriteBuffer);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to acquire write.\n");
        return;
    }
    memcpy(pWriteBuffer, pInput, framesToWrite * bytesPerFrame);
    result = ma_pcm_rb_commit_write(&ma_pcm_rb_in, framesToWrite);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to commit write.\n");
    }
    ma_uint32 frame = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float left = ABS(((float *)pInput)[frame]);
        float right = ABS(((float *)pInput)[frame + 1]);
        if (left > amplitude_in[0]) {
            amplitude_in[0] = left;
        }
        if (right > amplitude_in[1]) {
            amplitude_in[1] = right;
        }
        frame += 2;
    }
}

static void cb_capture_stop(ma_device *pDevice) {
    progress_left_in->value(0);
    progress_right_in->value(0);
}

static void cb_playback_stop(ma_device *pDevice) {
    progress_left_out->value(0);
    progress_right_out->value(0);
}

static void stop() {
    ma_device_uninit(&ma_device_in);
    ma_device_uninit(&ma_device_out);
    ma_pcm_rb_uninit(&ma_pcm_rb_in);
    recording = false;
    progress_input_delay->value(0);
    slider_input_delay->activate();
    input_delay->activate();
    choice_driver->activate();
    choice_input->activate();
    choice_output->activate();
    button_play->clear();
    button_play->label("Play (F5)");
}

static void cb_skip_stop(ma_device *pDevice) {
    ma_decoder_uninit(&ma_skip_decoder);
}

static void skipstop() {
    ma_device_uninit(&ma_device_skip);
    ma_device_set_master_volume(&ma_device_out, 1);
}

static void skip_finished(void *userData) {
    skipstop();
    button_skip->clear();
}

void cb_skip_data(ma_device *pDevice, void *pOutput, const void *pInput,
                  ma_uint32 frameCount) {
    ma_uint64 frames = 0;
    ma_result result = ma_decoder_read_pcm_frames(&ma_skip_decoder, pOutput, frameCount, &frames);
    // fprintf(stderr, "REQUEST FRAMES: %d READ FRAMES: %d\n", frameCount,
    // frames);
    if (frames < frameCount) {
        Fl::awake(skip_finished, 0);
    }
}

static void skip() {
    ma_decoder_config decoder_config = ma_decoder_config_init(
                                           DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE);
    ma_result result = ma_decoder_init_file(input_skipfile->value(),
                                            &decoder_config, &ma_skip_decoder);
    if (result != MA_SUCCESS) {
        fl_alert("Cannot open file: %s", input_skipfile->value());
        button_skip->clear();
        return;
    }
    ma_device_info *info_out =
        (ma_device_info *)choice_output->mvalue()->user_data();
    ma_device_config deviceConfig =
        ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_skip_decoder.outputFormat;
    deviceConfig.playback.channels = ma_skip_decoder.outputChannels;
    deviceConfig.playback.pDeviceID = &(info_out->id);
    deviceConfig.sampleRate = ma_skip_decoder.outputSampleRate;
    deviceConfig.dataCallback = cb_skip_data;
    deviceConfig.stopCallback = cb_skip_stop;
    if (ma_device_init(&context, &deviceConfig, &ma_device_skip) != MA_SUCCESS) {
        fl_alert("Failed to open playback device.");
        ma_decoder_uninit(&ma_skip_decoder);
        button_skip->clear();
        return;
    }
    if (ma_device_start(&ma_device_skip) != MA_SUCCESS) {
        fl_alert("Failed to start playback device.");
        ma_device_uninit(&ma_device_skip);
        ma_decoder_uninit(&ma_skip_decoder);
        button_skip->clear();
        return;
    }
    ma_device_set_master_volume(&ma_device_out, 0);
}

static void cb_button_skip(Fl_Light_Button *, void *) {
    if (ma_device_is_started(&ma_device_skip)) {
        skipstop();
    } else {
        skip();
    }
}

bool play() {
    if (recording) {
        return false;
    }
    playing = false;
    ma_uint32 input_delayFramecount =
        ma_calculate_buffer_size_in_frames_from_milliseconds(
            ma_uint32(input_delay->value() * 1000), DEVICE_SAMPLE_RATE);
    ma_device_info *info_in =
        (ma_device_info *)choice_input->mvalue()->user_data();
    ma_device_info *info_out =
        (ma_device_info *)choice_output->mvalue()->user_data();
    progress_input_delay->maximum(input_delay->value());
    ma_pcm_rb_init(DEVICE_FORMAT, DEVICE_CHANNELS, input_delayFramecount, NULL,
                   NULL, &ma_pcm_rb_in);
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = DEVICE_FORMAT;
    deviceConfig.capture.channels = DEVICE_CHANNELS;
    deviceConfig.capture.pDeviceID = &(info_in->id);
    deviceConfig.sampleRate = DEVICE_SAMPLE_RATE;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;
    deviceConfig.dataCallback = cb_capture;
    deviceConfig.stopCallback = cb_capture_stop;
    // deviceConfig.periods = 1;
    // deviceConfig.bufferSizeInFrames = 1000;
    deviceConfig.pUserData = NULL;
    ma_result result = ma_device_init(&context, &deviceConfig, &ma_device_in);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to initialize capture device.\n");
        button_play->clear();
        return false;
    }
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.playback.pDeviceID = &(info_out->id);
    deviceConfig.sampleRate = DEVICE_SAMPLE_RATE;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;
    deviceConfig.dataCallback = cb_playback;
    deviceConfig.stopCallback = cb_playback_stop;
    //  deviceConfig.pUserData = this;
    deviceConfig.pUserData = NULL;
    if (ma_device_init(&context, &deviceConfig, &ma_device_out) != MA_SUCCESS) {
        fl_alert("Failed to open playback device.\n");
        button_play->clear();
        return false;
    }
    amplitude_in[0] = 0;
    amplitude_in[1] = 0;
    amplitude_out[0] = 0;
    amplitude_out[1] = 0;
    if (ma_device_start(&ma_device_in) != MA_SUCCESS) {
        fl_alert("Failed to start recording device.\n");
        button_play->clear();
        return false;
    }
    ma_timer_init(&timer);
    recording = true;
    slider_input_delay->deactivate();
    choice_driver->deactivate();
    choice_input->deactivate();
    choice_output->deactivate();
    input_delay->deactivate();
    button_play->label("Stop (F5)");
    button_play->setonly();
    return true;
}

static void cb_button_play(Fl_Light_Button *, void *) {
    if (ma_device_is_started(&ma_device_in)) {
        stop();
    } else {
        play();
    }
}

void cb_timer(void *) {
    if (recording) {
        if (!playing) {
            double now = ma_timer_get_time_in_seconds(&timer);
            if (now >= input_delay->value()) {
                progress_input_delay->value(input_delay->value());
                playing = true;
                if (ma_device_start(&ma_device_out) != MA_SUCCESS) {
                    fl_alert("Failed to start playback device.\n");
                }
            } else {
                progress_input_delay->value(now);
            }
        }
        progress_left_in->value(amplitude_in[0]);
        progress_right_in->value(amplitude_in[1]);
        progress_left_out->value(amplitude_out[0]);
        progress_right_out->value(amplitude_out[1]);
        for (int i = 0; i < 2; i++) {
            amplitude_in[i] -= .05;
            amplitude_out[i] -= .05;
        }
    }
    Fl::repeat_timeout(TIMER, cb_timer);
}

static void cb_slider_input_delay(Fl_Slider *, void *) {
    input_delay->value(slider_input_delay->value());
}

static void cb_input_delay(Fl_Value_Input *, void *) {
    slider_input_delay->value(input_delay->value());
}

static void cb_skip_browse(Fl_Button *, void *) {
    Fl_Native_File_Chooser fnfc;
    fnfc.title("Pick a WAV file");
    fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    fnfc.filter("WAV Files\t*.wav");
    switch (fnfc.show()) {
        case -1:
            fl_alert("ERROR: %s", fnfc.errmsg());
            break; // ERROR
        case 1:
            break; // CANCEL
        default: {
            input_skipfile->value(fnfc.filename());
            break;
        } // FILE CHOSEN
    };
}

static void cb_about_close(Fl_Return_Button *, void *) {
    Fl::delete_widget(window_about);
}

static void cb_about_www(Fl_Button *, void *url) {
    fl_open_uri((char *)url);
}

void cb_about() {
    window_about = new Fl_Window(341, 252, "About RadioDelay");
    {
        Fl_Return_Button *o = new Fl_Return_Button(130, 205, 80, 30, "Close");
        o->callback((Fl_Callback *)cb_about_close);
    } // Fl_Return_Button* o
    {
        Fl_Button *o =
            new Fl_Button(25, 105, 295, 25, "Questions: info@@daansystems.com");
        o->callback((Fl_Callback *)cb_about_www,
                    (void *)"mailto:info@daansystems.com");
    } // Fl_Button* o
    {
        Fl_Button *o = new Fl_Button(25, 135, 295, 25, "Donate");
        o->callback((Fl_Callback *)cb_about_www,
                    (void *)"https://www.daansystems.com/donate");
    } // Fl_Button* o
    {
        Fl_Button *o =
            new Fl_Button(25, 165, 295, 25, "www.daansystems.com/radiodelay");
        o->callback((Fl_Callback *)cb_about_www,
                    (void *)"https://www.daansystems.com/radiodelay");
    } // Fl_Button* o
    Fl_Output *output_about = new Fl_Output(25, 50, 295, 50);
    output_about->type(12);
    output_about->box(FL_FLAT_BOX);
    output_about->color(FL_BACKGROUND_COLOR);
    output_about->labeltype(FL_NO_LABEL);
    window_about->end();
    int year;
    int day;
    char month[4];
    int monthnum;
    sscanf(__DATE__, "%4s %d %d", month, &day, &year);
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                         };
    for (monthnum = 0; monthnum < 12; monthnum++) {
        if (strcmp(months[monthnum], month) == 0)
            break;
    }
    char newtxt[255];
    snprintf(newtxt, 254,
             "RadioDelay Build %d%02d%02d %dbit\nCopyright 2006-2024 DaanSystems\nLicense: GPLv3",
             year, monthnum + 1, day, sizeof(size_t) == 8 ? 64 : 32);
    output_about->value(newtxt);
    window_about->show();
}

void init_mini_audio() {
    ma_context_config context_config = ma_context_config_init();
    int chosen_backend = enabledBackends[choice_driver->value()];

    ma_backend backends[] = {(ma_backend)chosen_backend};
    // context_config.alsa.useVerboseDeviceEnumeration = true;
    if (ma_context_init(backends, 1, &context_config, &context) != MA_SUCCESS) {
        fl_alert("Failed to initialize context.\n");
        return;
    }
    const char *backend = ma_get_backend_name(context.backend);
    ma_device_info *pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info *pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;
    ma_result result = ma_context_get_devices(
                           &context, &pPlaybackDeviceInfos, &playbackDeviceCount,
                           &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to retrieve device information.\n");
        return;
    }
    choice_input->clear();
    for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
        ma_device_info deviceInfo = pCaptureDeviceInfos[iDevice];
        //   fprintf(stderr, "IN: %u: %s  id: %x channels: %d sampleRate: %d\n",
        //   iDevice,
        //           deviceInfo.name, deviceInfo.id, deviceInfo.maxChannels,
        //           deviceInfo.minSampleRate);
        int index =
            choice_input->add("", 0, NULL, &(pCaptureDeviceInfos[iDevice]), 0);
        char name[255];
        snprintf(name, 254, "%s (%s)", deviceInfo.name, backend);
        choice_input->replace(index, name);
    }
    choice_output->clear();
    for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
        ma_device_info deviceInfo = pPlaybackDeviceInfos[iDevice];
        //    fprintf(stderr, "OUT: %u: %s  id: %x\n", iDevice,
        //            deviceInfo.name,
        //            deviceInfo.id);
        int index =
            choice_output->add("", 0, NULL, &(pPlaybackDeviceInfos[iDevice]), 0);
        char name[255];
        snprintf(name, 254, "%s (%s)", deviceInfo.name, backend);
        choice_output->replace(index, name);
    }
}

void driver_changed(Fl_Widget *, void *) {
    if (context.backend) {
        ma_context_uninit(&context);
    }
    init_mini_audio();
    choice_input->value(0);
    choice_output->value(0);
}

static int event_handler(int event) {
    if (event == FL_SHORTCUT) {
        const int key = Fl::event_key();
        switch (key) {
            case 65474: // F5
                cb_button_play(NULL, NULL);
                return 1;
            case 65475: // F6
                cb_button_skip(NULL, NULL);
                return 1;
        }
    }
    return 0;
}

void mic_permission_result(int result) {
    if (result == 0) {
        fl_alert("Please enable microphone permission");
    }
}

int main(int argc, char **argv) {
    int i = 1;
    setvbuf(stderr, NULL, _IONBF, 0);
    Fl::lock();
    if (Fl::args(argc, argv, i, arg_handler) < argc) {
        // note the concatenated strings to give a single format string!
        Fl::fatal("error: unknown option: %s\n"
                  "usage: %s [options]\n"
                  " -h | --help     : print extended help message\n"
                  " -driver # : driver \n"
                  " -in # : input device\n"
                  " -out # : output device\n"
                  " -play # : auto start playing\n"
                  " -delay # : set delay in seconds (f.e. 1.1)\n"
                  " -skipfile <path> : set the skipfile\n"
                  " plus standard FLTK options\n",
                  argv[i], argv[0]);
    }

    #ifdef __APPLE__
        extern void mic_permission(void (*callback)(int));
        mic_permission(mic_permission_result);
    #endif

    window_main = new Fl_Double_Window(702, 606, "RadioDelay");
    window_main->size_range(702, 606);
    window_main->align(Fl_Align(FL_ALIGN_CLIP | FL_ALIGN_INSIDE));
    {
        Fl_Box *o = new Fl_Box(15, 25, 670, 55, "Driver");
        o->box(FL_EMBOSSED_FRAME);
        o->align(Fl_Align(FL_ALIGN_TOP_LEFT));
    } // Fl_Box* o
    {
        choice_driver = new Fl_Choice(25, 40, 650, 25);
        choice_driver->down_box(FL_BORDER_BOX);
        choice_driver->labeltype(FL_NO_LABEL);
    } // Fl_Choice* input
    {
        Fl_Box *o = new Fl_Box(15, 100, 670, 110, "Input Device");
        o->box(FL_EMBOSSED_FRAME);
        o->align(Fl_Align(FL_ALIGN_TOP_LEFT));
    } // Fl_Box* o
    {
        choice_input = new Fl_Choice(25, 115, 650, 25);
        choice_input->down_box(FL_BORDER_BOX);
        choice_input->labeltype(FL_NO_LABEL);
    } // Fl_Choice* input
    {
        progress_left_in = new Fl_Progress(65, 150, 610, 25, "Left:");
        progress_left_in->selection_color(FL_SELECTION_COLOR);
        progress_left_in->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Progress* progress_left_in
    {
        progress_right_in = new Fl_Progress(65, 175, 610, 25, "Right:");
        progress_right_in->selection_color(FL_SELECTION_COLOR);
        progress_right_in->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Progress* progress_right_in
    {
        Fl_Box *o = new Fl_Box(15, 230, 670, 75, "Delay in seconds");
        o->box(FL_EMBOSSED_FRAME);
        o->align(Fl_Align(FL_ALIGN_TOP_LEFT));
    } // Fl_Box* o
    {
        slider_input_delay = new Fl_Slider(25, 240, 565, 25);
        slider_input_delay->type(5);
        slider_input_delay->box(FL_FLAT_BOX);
        slider_input_delay->minimum(0.01);
        slider_input_delay->maximum(3240);
        slider_input_delay->step(0.01);
        slider_input_delay->value(1);
        slider_input_delay->callback((Fl_Callback *)cb_slider_input_delay);
    } // Fl_Slider* slider_input_delay
    {
        input_delay = new Fl_Value_Input(595, 240, 80, 25);
        input_delay->labeltype(FL_NO_LABEL);
        input_delay->minimum(0.01);
        input_delay->maximum(3240);
        input_delay->step(0.01);
        input_delay->value(1);
        input_delay->callback((Fl_Callback *)cb_input_delay);
    } // Fl_Value_Input* input_delay
    {
        progress_input_delay = new Fl_Progress(25, 270, 650, 25);
        progress_input_delay->selection_color(FL_SELECTION_COLOR);
    } // Fl_Progress* progress_input_delay
    {
        Fl_Box *o = new Fl_Box(15, 325, 670, 110, "Output Device");
        o->box(FL_EMBOSSED_FRAME);
        o->align(Fl_Align(FL_ALIGN_TOP_LEFT));
    } // Fl_Box* o
    {
        choice_output = new Fl_Choice(25, 340, 650, 25);
        choice_output->down_box(FL_BORDER_BOX);
        choice_output->labeltype(FL_NO_LABEL);
    } // Fl_Choice* output
    {
        progress_left_out = new Fl_Progress(65, 375, 610, 25, "Left:");
        progress_left_out->selection_color(FL_SELECTION_COLOR);
        progress_left_out->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Progress* progress_left_out
    {
        progress_right_out = new Fl_Progress(65, 400, 610, 25, "Right:");
        progress_right_out->selection_color(FL_SELECTION_COLOR);
        progress_right_out->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Progress* progress_right_out
    {
        Fl_Box *o = new Fl_Box(15, 455, 670, 70, "Skip audio file");
        o->box(FL_EMBOSSED_FRAME);
        o->align(Fl_Align(FL_ALIGN_TOP_LEFT));
    } // Fl_Box* o
    {
        input_skipfile = new Fl_File_Input(30, 470, 540, 35);
    } // Fl_File_Input* input_skipfile
    {
        Fl_Button *o = new Fl_Button(580, 480, 95, 25, "Browse");
        o->callback((Fl_Callback *)cb_skip_browse);
    } // Fl_Button* o
    {
        button_play = new Fl_Light_Button(15, 535, 85, 60, "Play (F5)");
        button_play->callback((Fl_Callback *)cb_button_play);
    } // Fl_Light_Button* button_play
    {
        button_skip = new Fl_Light_Button(110, 535, 85, 60, "Skip (F6)");
        button_skip->callback((Fl_Callback *)cb_button_skip);
    } // Fl_Light_Button* o
    {
        button_mixer = new Fl_Button(475, 570, 100, 25, "Mixer");
        button_mixer->callback((Fl_Callback *)cb_mixer);
    } // Fl_Button* button_mixer
    {
        button_exit = new Fl_Button(585, 570, 100, 25, "Exit");
        button_exit->callback((Fl_Callback *)cb_exit);
    } // Fl_Button* button_exit
    {
        Fl_Button *o = new Fl_Button(585, 535, 100, 25, "About...");
        o->callback((Fl_Callback *)cb_about);
    } // Fl_Button* o
    window_main->end();
    window_main->resizable(window_main);
    {
        size_t enabledBackendCount;
        ma_result result = ma_get_enabled_backends(enabledBackends, MA_BACKEND_COUNT, &enabledBackendCount);
        if (result != MA_SUCCESS) {
            fl_alert("Failed to get enabled drivers");
            return 1;
        }
        for (ma_uint32 iBackend = 0; iBackend < enabledBackendCount; ++iBackend) {
            ma_backend backend = enabledBackends[iBackend];
            const char *backend_name = ma_get_backend_name(backend);
            int index = choice_driver->add("", 0, driver_changed, NULL, 0);
            choice_driver->replace(index, backend_name);
        }
    }
    if (opts.driver > 0) {
        choice_driver->value(opts.driver);
    } else {
        choice_driver->value(0);
    }
    init_mini_audio();
    if (opts.in > 0) {
        choice_input->value(opts.in);
    } else {
        choice_input->value(0);
    }
    if (opts.out > 0) {
        choice_output->value(opts.out);
    } else {
        choice_output->value(0);
    }
    if (opts.delay > 0) {
        input_delay->value(opts.delay);
        slider_input_delay->value(opts.delay);
    }
    progress_left_in->maximum(1);
    progress_right_in->maximum(1);
    progress_left_out->maximum(1);
    progress_right_out->maximum(1);
    Fl::add_timeout(TIMER, cb_timer);
    if (opts.play) {
        play();
    }
    if (opts.skipfile) {
        input_skipfile->value(opts.skipfile);
    }
#ifdef WIN32
    char *my_icon = (char *)LoadIcon(fl_display, MAKEINTRESOURCE(1000));
    window_main->icon(my_icon);
#elif __linux__
    Fl_Pixmap pixmap(radiodelay);
    Fl_RGB_Image my_icon(&pixmap);
    window_main->icon(&my_icon);
#elif __APPLE__
    fl_mac_set_about((Fl_Callback *)cb_about, NULL, 0);
#endif
    Fl::add_handler(event_handler);
    window_main->show(argc, argv);
    return Fl::run();
}
