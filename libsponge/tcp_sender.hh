#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cassert>
#include <functional>
#include <queue>
#include <set>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
struct cmp {
    bool operator()(const TCPSegment &a, const TCPSegment &b) const {
        return a.header().seqno.raw_value() < b.header().seqno.raw_value();
    }
};
class TCPRetransmissionTimer {
  public:
    //! retransmission timer for the connection
    unsigned int _initial_RTO;

    //! retransmission timeout
    unsigned int _RTO;

    //! timeout
    unsigned int _TO;

    //! state of the timer, 1:open, 0:close
    bool _open;

    //! Initialize a TCP retransmission timer
    TCPRetransmissionTimer(const uint16_t retx_timeout)
        : _initial_RTO(retx_timeout), _RTO(retx_timeout), _TO(0), _open(1) {}

    //! state of the timer
    bool open() { return _open; }

    //! start the timer
    void start() {
        _open = 1;
        _TO = 0;
    }

    //! close the timer
    void close() {
        _open = 0;
        _TO = 0;
    }

    //! tick
    bool tick(size_t &ms_since_last_tick) {
        if (!open())
            return 0;
        if(ms_since_last_tick > _RTO - _TO){
            ms_since_last_tick -= (_RTO - _TO);
            _TO=_RTO;
        }
        else{
            _TO += ms_since_last_tick;
            ms_since_last_tick = 0;
        }
        if (_TO >= _RTO) {
            _TO = 0;
            return 1;  // the retransmission timer has expired.
        }
        return 0;
    }
};

class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out;

    //! outstanding segments that the TCPSender may resend
    std::set<TCPSegment, cmp> _segments_outstanding;

    //! bytes in flight
    uint64_t _nBytes_inflight;

    //！ last ackno
    uint64_t _recv_ackno;

    //! TCP retransmission timer
    TCPRetransmissionTimer _timer;

    //! notify the window size
    uint16_t _window_size;

    //! consecutive retransmissions
    unsigned int _consecutive_retransmissions;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno;

    //! the flag of SYN sent
    bool _syn_sent;

    //! the flag of FIN sent
    bool _fin_sent;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    bool syn_sent() const {return _syn_sent;}

    bool fin_sent() const {return _fin_sent;}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
