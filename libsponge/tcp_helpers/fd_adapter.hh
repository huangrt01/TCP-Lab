#ifndef SPONGE_LIBSPONGE_FD_ADAPTER_HH
#define SPONGE_LIBSPONGE_FD_ADAPTER_HH

#include "file_descriptor.hh"
#include "lossy_fd_adapter.hh"
#include "socket.hh"
#include "tcp_config.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <optional>
#include <utility>

//! \brief Basic functionality for file descriptor adaptors
//! \details See TCPOverUDPSocketAdapter and TCPOverIPv4OverTunFdAdapter for more information.
class FdAdapterBase {
  private:
    FdAdapterConfig _cfg{};  //!< Configuration values
    bool _listen = false;    //!< Is the connected TCP FSM in listen state?

  protected:
    FdAdapterConfig &config_mutable() { return _cfg; }

  public:
    //! \brief Set the listening flag
    //! \param[in] l is the new value for the flag
    void set_listening(const bool l) { _listen = l; }

    //! \brief Get the listening flag
    //! \returns whether the FdAdapter is listening for a new connection
    bool listening() const { return _listen; }

    //! \brief Get the current configuration
    //! \returns a const reference
    const FdAdapterConfig &config() const { return _cfg; }

    //! \brief Get the current configuration (mutable)
    //! \returns a mutable reference
    FdAdapterConfig &config_mut() { return _cfg; }
};

//! \brief A FD adaptor that reads and writes TCP segments in UDP payloads
class TCPOverUDPSocketAdapter : public FdAdapterBase, public UDPSocket {
  public:
    //! Construct from a UDPSocket sliced into a FileDescriptor
    explicit TCPOverUDPSocketAdapter(FileDescriptor &&fd) : UDPSocket(std::move(fd)) {}

    //! Attempts to read and return a TCP segment related to the current connection from a UDP payload
    std::optional<TCPSegment> read();

    //! Writes a TCP segment into a UDP payload
    void write(TCPSegment &seg);
};

//! Typedef for TCPOverUDPSocketAdapter
using LossyTCPOverUDPSocketAdapter = LossyFdAdapter<TCPOverUDPSocketAdapter>;

#endif  // SPONGE_LIBSPONGE_FD_ADAPTER_HH
