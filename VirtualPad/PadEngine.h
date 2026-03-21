#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Runs the gamepad read → macro → ViGEm pipeline on a background thread.
// The main (UI) thread reads engine status via the thread-safe accessors below.
class PadEngine {
public:
    PadEngine();
    ~PadEngine();

    void start();   // spawn background thread
    void stop();    // signal thread to stop and wait for it to finish

    // Thread-safe status accessors (called from the UI thread)
    bool        isRunning()   const { return m_running.load(); }
    bool        isConnected() const { return m_connected.load(); }
    std::string getDevice()   const;
    std::string getStatus()   const;

private:
    void threadFunc();

    std::thread        m_thread;
    std::atomic<bool>  m_running   { false };
    std::atomic<bool>  m_connected { false };

    mutable std::mutex m_mutex;
    std::string        m_device;   // name of the active input device
    std::string        m_status;   // one-line human-readable status

    void setDevice(const std::string& s);
    void setStatus(const std::string& s);
};
