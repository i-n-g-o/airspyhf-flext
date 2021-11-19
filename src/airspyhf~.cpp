#include "airspyhf~.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <iomanip>

#define MAXIMUM_OVERSAMPLE 16

static int init_count = 0;

static int airspyhfSampleCb(airspyhf_transfer_t* transfer_fn)
{
    /*

    typedef struct {
        airspyhf_device_t* device;
        void* ctx;
        airspyhf_complex_float_t* samples;
        int sample_count;
        uint64_t dropped_samples;
    } airspyhf_transfer_t;

    */

    if (transfer_fn != NULL)
    {
        airspyhf *cls = (airspyhf *)transfer_fn->ctx;
        cls->addData(transfer_fn->samples, transfer_fn->sample_count);
    }

    return 0;
}

static void postThreadId(const char* prefix)
{
    std::thread::id this_id = std::this_thread::get_id();
    std::cout << prefix << this_id << std::endl;

    // std::stringstream ss;
    // ss << std::this_thread::get_id();
    // post("thread id: %lld", std::stoull(ss.str()));

    // auto id = std::hash<std::thread::id>()(std::this_thread::get_id());
    // post("thread id: %ld", id);
}

airspyhf::airspyhf()
    : flext_dsp()
    , m_device(nullptr)
    , m_running(false)
    , m_samplesWritten(false)
    , m_write_idx(0)
    , m_read_idx(0)
    , m_nth(48000.0 / 44100.0) //4.3543 4.276363636363636
    , m_nthCount(0)
    , m_sampleCount(0)
{
    AddInAnything();
    AddOutSignal("I");
    AddOutSignal("Q");
    // outputs for write and read idx
    AddOutInt("write index");
    AddOutInt("read index");

    FLEXT_ADDMETHOD_(0, "devices", listDevices);
    FLEXT_ADDMETHOD_(0, "open", open);
    FLEXT_ADDMETHOD_(0, "close", close);
    FLEXT_ADDMETHOD_(0, "start", start);
    FLEXT_ADDMETHOD_(0, "stop", stop);
    FLEXT_ADDMETHOD_(0, "version", version);
    FLEXT_ADDMETHOD_(0, "info", info);

    FLEXT_ADDMETHOD_I(0, "freq", setFreq);
    FLEXT_ADDMETHOD_I(0, "samplerate", setSamplerate);
    FLEXT_ADDMETHOD_I(0, "agc", setAgc);
    FLEXT_ADDMETHOD_I(0, "calibration", setCalibration);
    FLEXT_ADDMETHOD_F(0, "iqcorr", setIqCorr);
    FLEXT_ADDMETHOD_I(0, "att", setAtt);
    FLEXT_ADDMETHOD_I(0, "lna", setLna);
    FLEXT_ADDMETHOD_F(0, "nth", setNth);
    

    postThreadId("constructor thread: ");

    m_last_data.im = 0;
    m_last_data.re = 0;
}

airspyhf::~airspyhf()
{
    close();
}

bool airspyhf::CbDsp()
{
    // yes, enable dsp
    return true;
}

void airspyhf::CbSignal()
{
    int n = Blocksize();
    t_sample *const* ins = InSig();
    t_sample *const* outs = OutSig();

    if (m_running && m_samplesWritten)
    {
        m_mutex.lock();

        t_sample *out1 = outs[0];
        t_sample *out2 = outs[1];

        for(int i = 0; i < n; i++) 
        {
            airspyhf_complex_float_t& sample = m_buffer[m_read_idx];
            m_read_idx++;
            if (m_read_idx >= m_buffersize)
            {
                m_read_idx = 0;
            }

            // out 1: I
            *out1 = sample.re;
            // out 2: Q
            *out2 = sample.im;

            out1++;
            out2++;

            sample.re = 0;
            sample.im = 0;
        }

        ToOutInt(3, m_read_idx);

        m_mutex.unlock();
    }
    else
    {
        for(int i = 0; i < CntOutSig(); i++) ZeroSamples(outs[i],n);
        ToOutInt(3, m_read_idx);
    }
}



void airspyhf::addData(airspyhf_complex_float_t* data, int size)
{
    m_mutex.lock();

    int nthCount_i = int(m_nthCount);
    double rest = 0;    

    for (int i = 0; i < size; i++)
    {
        m_sampleCount++;

        if (m_sampleCount >= nthCount_i)
        {
            // add that sample
            rest = m_nthCount - nthCount_i;

            m_buffer[m_write_idx].im = ((1.0 - rest) * m_last_data.im) + (rest * data[i].im);
            m_buffer[m_write_idx].re = ((1.0 - rest) * m_last_data.re) + (rest * data[i].re);

            m_write_idx++;
            if (m_write_idx >= m_buffersize)
            {
                m_write_idx = 0;
            }

            m_nthCount += m_nth;
            nthCount_i = int(m_nthCount);
        }

        m_last_data = data[i];
    }

    // std::cout << std::fixed << std::setprecision(3);
    // std::cout << "sample count: " << m_sampleCount << " | " << std::setw(7) << m_nthCount << std::endl;

    ToOutInt(2, m_write_idx);

    if (!m_samplesWritten)
    {
        if (init_count >= MAXIMUM_OVERSAMPLE/2)
        {
            m_samplesWritten = true;
        }
        else
        {
            init_count++;
        }
    }

    if (m_samplesWritten)
    {
        // keep these numbers small
        while (m_sampleCount > 10000 && m_nthCount > 10000)
        {
            m_sampleCount -= 10000;
            m_nthCount -= 10000;
        }
    }

    m_mutex.unlock();
}

// void airspyhf::m_signal(int n, float *const *in, float *const *out)
// {
//     float *out1 = out[0];
//     float *out2 = out[1];
    
//     if (m_running && m_samplesWritten)
//     {
//         m_mutex.lock();
        
//         while (n--)
//         {            
//             airspyhf_complex_float_t& sample = m_buffer[m_read_idx++];
//             if (m_read_idx >= m_buffersize) 
//             {
//                 m_read_idx = 0;
//             }

//             // out 1: I
//             *out1++ = sample.re;
//             // out 2: Q
//             *out2++ = sample.im;

//             sample.re = 0;
//             sample.im = 0;
//         }

//         ToOutInt(3, m_read_idx);

//         m_mutex.unlock();
//     }
//     else
//     {
//         while (n--)
//         {
//             // out 1: I
//             *out1++ = 0;
//             // out 2: Q
//             *out2++ = 0;
//         }
//     }
// }

void airspyhf::listDevices()
{
    uint64_t devices[16];
    int count = airspyhf_list_devices(devices, 16);

    post("devices: %d", count);

    for (int i=0; i<count; i++)
    {
        post("device [%d]: %ld", i, devices[i]);
    }
}

void airspyhf::open()
{
    if (m_device)
    {
        post("device already open");
        return;
    }

    int r = airspyhf_open(&m_device);

    if (r != 0)
    {
        error("could not open device");
        return;
    }
    

    //int r = airspyhf_open_sn(airspyhf_device_t** device, uint64_t serial_number);

    if (m_device)
    {
        /* Returns the number of IQ samples to expect in the callback */
        int samples = airspyhf_get_output_size(m_device);
        m_buffersize = samples * MAXIMUM_OVERSAMPLE;

        post("expected samples: %d - %d", samples, m_buffersize);
        // resize buffer
        m_buffer.resize(m_buffersize);

        // set agc
        r = airspyhf_set_hf_agc(m_device, 1);
        post("agc result: %d", r);

        r = airspyhf_set_samplerate(m_device, 44100);
        post("samplerate result: %d", r);

        info();
    }
    else
    {
        error("could not open device");
    }
}

void airspyhf::close()
{
    stop();

    if (m_device)
    {
        int r = airspyhf_close(m_device);
        m_device = nullptr;
    }
}

void airspyhf::signalThread()
{
    postThreadId("signal thread: ");

    m_running = true;
    int r = airspyhf_start(m_device, airspyhfSampleCb, this);
    post("start: %d", r);

    post("thread done");
}

void airspyhf::testThread()
{
    m_running = true;

    while(m_do)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        post("wake");
    }
    post("test thread done");

    m_running = false;
}

void airspyhf::start()
{
    if (m_running) 
    {
        post("already running");
        return;
    }

    if (m_device == nullptr)
    {
        open();
    }

    // m_running = true;
    // m_do = true;
    // m_deviceThread = std::thread(&airspyhf::testThread, this);

    if (m_device)
    {
        postThreadId("start signal thread: ");

        m_write_idx = 0;
        m_read_idx = 0;
        init_count = 0;
        m_nthCount = m_nth;
        m_sampleCount = 0;
        m_samplesWritten = false;
        m_deviceThread = std::thread(&airspyhf::signalThread, this);
    }
}

void airspyhf::stop()
{
    if (m_device)
    {
        if (m_deviceThread.native_handle())
        {
            // m_do = false;
            int r = airspyhf_stop(m_device);   
            post("stop result: %d", r); 
            
            m_deviceThread.join();
        }
    }

    m_running = false;
    m_samplesWritten = false;
}

void airspyhf::version()
{
    airspyhf_lib_version_t version;
    airspyhf_lib_version(&version);
    post("airspyhf library version: %d.%d.%d", version.major_version, version.minor_version, version.revision);
}

void airspyhf::info()
{
    post("\n------ info -----");

    if (m_device)
    {        
        // serial number
        airspyhf_read_partid_serialno_t device_info;
        int r = airspyhf_board_partid_serialno_read(m_device, &device_info);
        post("searialno_read result = %d", r);
        post("device part id: %d", device_info.part_id);
        post("device serial no: %d %d %d %d", device_info.serial_no[0], device_info.serial_no[1], device_info.serial_no[2], device_info.serial_no[3]);

        // version
        char ver[255];
        r = airspyhf_version_string_read(m_device, ver, 255);
        post("device version result: %d", r);
        post("device version: %s", ver);

        // is streaming
        int is_streaming = airspyhf_is_streaming(m_device);
        post("is streaming: %d", is_streaming);

        // get is low_if
        int low_if = airspyhf_is_low_if(m_device);
        if (low_if == 0)
        {
            post("Zero-IF");
        }
        else
        {
            post("Low-IF");
        }

        // get sample-rates
        uint32_t buf[8];
        memset(buf, 0, 8 * sizeof(uint32_t));
        r = airspyhf_get_samplerates(m_device, buf, 8);
        post("get samplerates result: %d", r);        
        for(int i=0; i<8; i++)
        {
            post("SR [%d]: %d", i, buf[i]);
        }

        // get calibration
        int32_t ppb = 0;
        r = airspyhf_get_calibration(m_device, &ppb);
        post("get calibration result: %d", r);
        post("get calibration: %d", ppb);
    }
    else
    {
        post("no open device");
    }
}


void airspyhf::setFreq(const uint32_t freq)
{
    if (m_device)
    {
        int r = airspyhf_set_freq(m_device, freq);
        if (r != 0)
        {
            error("could not set frequency: %d", freq);
        }
    }
    else
    {
        post("no device to set freq: %d", freq);
    }
}

void airspyhf::setSamplerate(uint32_t samplerate)
{
    post("set samplerate: %d", samplerate);
    setNth(4 * 192000.0 / double(samplerate));
    // if (m_device)
    // {
    //     post("set samplerate: %d", samplerate);
    //     int r = airspyhf_set_samplerate(m_device, samplerate);
    //     post("samplerate result: %d", r);
    // }
    // else
    // {
    //     error("no device to set samplerate: %d", samplerate);
    // }
}

void airspyhf::setAgc(uint32_t agc)
{
    if (m_device)
    {
        post("set agc: %d", agc);
        int r = airspyhf_set_hf_agc(m_device, (agc > 0 ? 1 : 0));
        post("agc result: %d", r);
    }
    else
    {
        error("no device to set agc: %d", agc);
    }
}

void airspyhf::setCalibration(uint32_t calibration)
{
    if (m_device)
    {
        post("set calibration: %d", calibration);
        int r = airspyhf_set_calibration(m_device, calibration);
        post("calibration result: %d", r);
    }
    else
    {
        error("no device to set calibration: %d", calibration);
    }
}

void airspyhf::setIqCorr(float value)
{
    if (m_device)
    {
        post("set iq corr point: %f", value);
        int r = airspyhf_set_optimal_iq_correction_point(m_device, value);
        post("iq corr result: %d", r);
    }
    else
    {
        error("no device to set iq corr: %f", value);
    }
}

void airspyhf::setAtt(uint32_t value)
{
    uint8_t v = value;
    if (v > 8)
    {
        v = 8;
    }

    if (m_device)
    {
        post("set attenuation: %d", v);
        int r = airspyhf_set_hf_att(m_device, v);
        post("att result: %d", r);
    }
    else
    {
        error("no device to set att: %d", v);
    }
}

void airspyhf::setLna(uint32_t value)
{
    uint8_t v = value > 0 ? 1 : 0;

    if (m_device)
    {
        post("set Lna: %d", v);
        int r = airspyhf_set_hf_lna(m_device, v);
        post("lna result: %d", r);
    }
    else
    {
        error("no device to set lna: %d", value);
    }
}

void airspyhf::setNth(float value)
{
    init_count = 0;
    m_samplesWritten = false;

    post("nth: %f", value);
    m_nth = value;

    m_write_idx = 0;
    m_read_idx = 0;
    m_nthCount = m_nth;
    m_sampleCount = 0;
}

FLEXT_NEW_DSP("airspyhf~", airspyhf);