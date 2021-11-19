#ifndef AIRSPYHF_H
#define AIRSPYHF_H

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

#include <flext.h>

#if !defined(FLEXT_VERSION) || (FLEXT_VERSION < 401)
#error You need at least flext version 0.4.1
#endif

#include <thread>

#include <airspyhf.h>

class airspyhf : public flext_dsp
{
    FLEXT_HEADER(airspyhf, flext_dsp)

public:
    airspyhf();
    ~airspyhf();

protected:
    // virtual void m_signal(int n, float *const *in, float *const *out);
    virtual bool CbDsp() override;
    virtual void CbSignal() override;

protected:
    void listDevices();
    void open();
    void close();
    void start();
    void stop();
    void version();
    void info();

    void setFreq(const uint32_t freq);
    void setSamplerate(uint32_t samplerate);
    void setAgc(uint32_t agc);
    void setCalibration(uint32_t calibration);
    void setIqCorr(float value);
    void setAtt(uint32_t value);
    void setLna(uint32_t value);
    void setNth(float value);

private:
    FLEXT_CALLBACK(listDevices)
    FLEXT_CALLBACK(open)
    FLEXT_CALLBACK(close)
    FLEXT_CALLBACK(start)
    FLEXT_CALLBACK(stop)
    FLEXT_CALLBACK(version)
    FLEXT_CALLBACK(info)

    FLEXT_CALLBACK_I(setFreq)
    FLEXT_CALLBACK_I(setSamplerate)
    FLEXT_CALLBACK_I(setAgc)
    FLEXT_CALLBACK_I(setCalibration)
    FLEXT_CALLBACK_F(setIqCorr)
    FLEXT_CALLBACK_I(setAtt)
    FLEXT_CALLBACK_I(setLna)
    FLEXT_CALLBACK_F(setNth)

public:
    void signalThread();
    void testThread();

    // called by sample-thread
    void addData(airspyhf_complex_float_t* data, int size);

private:
    airspyhf_device_t* m_device;
    std::atomic_bool m_running;
    std::atomic_bool m_do;
    std::atomic_bool m_samplesWritten;

    std::thread m_deviceThread;
    std::mutex m_mutex;

    std::vector<airspyhf_complex_float_t> m_buffer;
    size_t m_write_idx;
    size_t m_read_idx;
    
    std::atomic<double> m_nth;
    double m_nthCount;
    int m_sampleCount;

    airspyhf_complex_float_t m_last_data;
    size_t m_buffersize;
};

#endif // AIRSPYHF_H
