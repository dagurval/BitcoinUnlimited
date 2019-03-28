#ifndef ELECTRUM_ELECTRUMSERVER_H
#define ELECTRUM_ELECTRUMSERVER_H

#include <boost/process.hpp>

namespace electrum {

class ElectrumServer {
public:
    static ElectrumServer& Instance();
    bool Start(int rpcport, const std::string& network);
    void Stop();
    ~ElectrumServer() { if (started) Stop(); }

private:
    ElectrumServer() { }

    bool started = false;
    boost::process::child process;

    boost::process::ipstream p_stdout;
    boost::process::ipstream p_stderr;
    std::thread stdout_reader_thread;
    std::thread stderr_reader_thread;
};

} // ns electrum

#endif
