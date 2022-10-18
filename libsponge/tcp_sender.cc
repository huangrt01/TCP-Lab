#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _segments_out{}
    , _segments_outstanding{}
    , _nBytes_inflight(0)
    , _recv_ackno(0)
    , _timer{retx_timeout}
    , _window_size(1)
    , _consecutive_retransmissions{0}
    , _stream(capacity)
    , _next_seqno(0)
    , _syn_sent(0)
    , _fin_sent(0) {}

uint64_t TCPSender::bytes_in_flight() const { return _nBytes_inflight; }

void TCPSender::fill_window() {
    assert(!_stream.error());
    TCPSegment seg;
    if (_next_seqno == 0) {
        // state is CLOSE, need to send SYN
        seg.header().syn = 1;
        _syn_sent = 1;
        send_non_empty_segment(seg);
        return;
    } else if (_next_seqno == _nBytes_inflight) {
        // state is SYN SENT, don't send SYN
        return;
    }

    // zero window probing
    uint16_t window_size = _window_size ? _window_size : 1;
    uint64_t remaining;
    while ((remaining = static_cast<uint64_t>(window_size) + (_recv_ackno - _next_seqno))) {
        // FIN flag occupies space in window
        TCPSegment new_seg;
        if (_stream.eof() && !_fin_sent) {
            new_seg.header().fin = 1;
            _fin_sent = 1;
            send_non_empty_segment(new_seg);
            return;
        } else if (_stream.eof()) {
            return;
        }
        size_t size = min(static_cast<size_t>(remaining), TCPConfig::MAX_PAYLOAD_SIZE);
        new_seg.payload() = Buffer(_stream.read(size));
        if (new_seg.length_in_sequence_space() < window_size && _stream.eof()) {  // piggy-back FIN
            new_seg.header().fin = 1;
            _fin_sent = 1;
        }
        if (new_seg.length_in_sequence_space() == 0) {
            return;
        }
        send_non_empty_segment(new_seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno - next_seqno() > 0) {
        return false;
    }
    //如果window_size为0，需要记录下来，"zero window probing", 影响tick()和fill_window()的行为
    _window_size = window_size;
    uint64_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    if (abs_ackno <= _recv_ackno) {
        return true;
    }

    _recv_ackno = abs_ackno;
    // acknowledges the successful receipt of new data
    _timer._RTO = _timer._initial_RTO;
    _timer._TO = 0;
    _consecutive_retransmissions = 0;

    // deque fully-acknowledged segments
    while (!_segments_outstanding.empty()) {
        const auto &seg = _segments_outstanding.front();
        if (ackno - seg.header().seqno >= static_cast<int32_t>(seg.length_in_sequence_space())) {
            _nBytes_inflight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        } else {
            break;
        }
    }

    // fill the window
    fill_window();

    // any outstanding segment, restart the timer.
    // [RFC6298](5.3)
    if (!_segments_outstanding.empty()) {
        _timer.start();
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    size_t time_left = ms_since_last_tick;
    if (_timer.tick(time_left)) {
        // Notice: remove fill_the_window() here to fix the test fsm_retx_relaxed
        // timer has expired, retransmit at most ONE outstanding segment
        if (!_segments_outstanding.empty()) {
            // retransmit the outstanding segment with the lowest sequence number
            _segments_out.push(_segments_outstanding.front());
            if (_window_size) {
                _consecutive_retransmissions++;
                _timer._RTO *= 2;  // double the RTO, exponential backoff, it slows down retransmissions on lousy
                                   // networks to avoid further gumming up the works
            }
            if (!_timer.open()) {  //[RFC6298](5.1)
                _timer.start();
            }
            if (syn_sent() && (_next_seqno == _nBytes_inflight) && (_timer._RTO < 3000)) {
                _timer._RTO = 3000;  // SYN_SENT, [RFC6298](5.7)
            }
        }
        if (_segments_outstanding.empty()) {
            _timer.close();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::send_non_empty_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);

    _next_seqno += seg.length_in_sequence_space();
    _nBytes_inflight += seg.length_in_sequence_space();
    // std::cerr << "send non empty: " << seg.header().to_string() << "length:" << seg.length_in_sequence_space() <<
    // endl << endl;
    _segments_out.push(seg);
    _segments_outstanding.push(seg);

    // [RFC6298]:(5.1)
    if (!_timer.open()) {
        _timer.start();
    }
}
