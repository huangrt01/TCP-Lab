#ifndef SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
#define SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH

#include "fd_adapter.hh"
#include "file_descriptor.hh"
#include "lossy_fd_adapter.hh"
#include "tcp_segment.hh"

#include <optional>
#include <utility>

//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter : public FdAdapterBase, public FileDescriptor {
  public:
    //! Construct from a TunFD sliced into a FileDescriptor
    explicit TCPOverIPv4OverTunFdAdapter(FileDescriptor &&fd) : FileDescriptor(std::move(fd)) {}

    //! Attempts to read and parse an IPv4 datagram containing a TCP segment related to the current connection
    std::optional<TCPSegment> read();

    //! Creates an IPv4 datagram from a TCP segment and writes it to the TUN device
    void write(TCPSegment &seg);
};

//! Typedef for TCPOverIPv4OverTunFdAdapter
using LossyTCPOverIPv4OverTunFdAdapter = LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;

#endif  // SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
