#include "electrum/electrumserver.h"
#include "electrum/electrs.h"
#include "util.h"
#include "utilprocess.h"

#include <chrono>
#include <string>

#include <boost/process/child.hpp>
#include <boost/process/pipe.hpp>
#include <boost/process/io.hpp>

namespace bp = boost::process;

static void log_pipe(bp::ipstream &pipe)
{
    std::string line;
    while (pipe && std::getline(pipe, line))
    {
        LOG(ELECTRUM, "Electrum: %s", line);
    }
}

//! give the program a second to complain about startup issues, such as invalid
//! parameters.
static bool startup_check(bp::child &process)
{
    auto timeout = std::chrono::seconds(1);
    if (!process.wait_for(timeout))
    {
        // process hasn't exited, good.
        return true;
    }
    LOGA("Electrum: server exited with exit code: %d", process.exit_code());
    return false;
}

static void log_args(const std::string &path, const std::vector<std::string> &args)
{
    if (!Logging::LogAcceptCategory(ELECTRUM))
    {
        return;
    }

    std::stringstream ss;
    ss << path;
    for (auto &a : args)
    {
        ss << " " << a;
    }
    LOGA("Electrum: spawning %s", ss.str());
}

namespace electrum
{

// This structs sole purpose is to avoid including boost process in the header
// file.
struct Process {
    bp::child process;
    bp::ipstream p_stdout;
    bp::ipstream p_stderr;
};

ElectrumServer::ElectrumServer() : process(new electrum::Process) { }
ElectrumServer::~ElectrumServer() { if (started) Stop(); }

bool ElectrumServer::Start(int rpcport, const std::string &network)
{
    assert(!started);
    if (!GetBoolArg("-electrum", false))
    {
        LOGA("Electrum: Disabled. Not starting server.");
        return true;
    }
    LOGA("Electrum: Starting server");

    try
    {
        auto path = electrs_path();
        auto args = electrs_args(rpcport, network);
        log_args(path, args);
        process->process = bp::child(path, args,
                bp::std_out > process->p_stdout,
                bp::std_err > process->p_stderr);
    }
    catch (const std::exception &e)
    {
        LOGA("Electrum: Unable to start server: %s", e.what());
        return false;
    }

    stderr_reader_thread = std::thread([this]() {
        LOGA("Electrum: stderr log thread started.");
        RenameThread("electrumstderr");
        log_pipe(this->process->p_stderr);
    });

    if (Logging::LogAcceptCategory(ELECTRUM))
    {
        stdout_reader_thread = std::thread([this]() {
            LOG(ELECTRUM, "Electrum: stdout log thread started.");
            RenameThread("electrumstdout");
            log_pipe(this->process->p_stdout);
        });
    }
    started = true;
    return startup_check(process->process);
}

void ElectrumServer::Stop()
{
    if (!started)
    {
        return;
    }
    LOGA("Electrum: Stopping server");

    try
    {
        send_signal_sigterm(process->process.id());
        auto timeout = std::chrono::seconds(60);
        if (!process->process.wait_for(timeout))
        {
            LOGA("Electrum: Timed out waiting for clean shutdown (%s seconds)", timeout.count());
        }
    }
    catch (const std::exception &e)
    {
        LOGA("Electrum: %s", e.what());
    }

    process->process.terminate();
    // reader threads exit when pipes close
    process->p_stdout.pipe().close();
    process->p_stderr.pipe().close();
    if (stdout_reader_thread.joinable())
    {
        stdout_reader_thread.join();
    }
    if (stderr_reader_thread.joinable())
    {
        stderr_reader_thread.join();
    }

    started = false;
}

ElectrumServer &ElectrumServer::Instance()
{
    static ElectrumServer instance;
    return instance;
}

} // ns electrum
