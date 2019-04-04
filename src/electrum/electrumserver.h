#ifndef ELECTRUM_ELECTRUMSERVER_H
#define ELECTRUM_ELECTRUMSERVER_H

#include <memory>
#include <thread>

namespace electrum {

struct Process;

class ElectrumServer {
public:
    static ElectrumServer& Instance();
    bool Start(int rpcport, const std::string& network);
    void Stop();
    ~ElectrumServer();

private:
    ElectrumServer();

    std::unique_ptr<Process> process;
    std::thread stdout_reader_thread;
    std::thread stderr_reader_thread;

    bool started = false;
};

} // ns electrum

#endif
