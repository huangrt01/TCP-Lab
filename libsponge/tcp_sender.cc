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
        _syn_sent=1;
    } else if (_next_seqno == _nBytes_inflight) {
        // state is SYN SENT, don't send SYN
        return;
    } else {
        uint16_t win = _window_size;
        if (_window_size == 0)
            win = 1;  // zero window probing
        uint16_t remaining = win + static_cast<uint16_t>(_recv_ackno - _next_seqno);
        if (_stream.eof() && !_fin_sent) {
            if (remaining == 0)
                return;  // FIN flag occupies space in window
            seg.header().fin = 1;
            _fin_sent = 1;
        } else if (_stream.eof())
            return;
        else {  // SYN_ACKED
            uint16_t size = min(remaining, static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE));
            seg.payload() = Buffer(std::move(_stream.read(size)));
            if (seg.length_in_sequence_space() < win && _stream.eof()) {  // piggy-back FIN
                seg.header().fin = 1;
                _fin_sent = 1;
            }
            if (seg.length_in_sequence_space() == 0)
                return;
        }
    }
    seg.header().win = _window_size;
    seg.header().seqno = wrap(_next_seqno, _isn);

    _next_seqno += seg.length_in_sequence_space();
    _nBytes_inflight += seg.length_in_sequence_space();

    _segments_out.push(seg);
    _segments_outstanding.insert(seg);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //如果window_size为0，需要记录下来，"zero window probing", 影响tick()和fill_window()的行为
    _window_size = window_size;

    uint64_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    if (ackno - next_seqno() > 0)
        return 0;
    if (abs_ackno - _recv_ackno <= 0)
        return 1;

    // acknowledges the successful receipt of new data
    _timer._RTO = _timer._initial_RTO;
    _consecutive_retransmissions = 0;
    _recv_ackno = abs_ackno;

    //删掉fully-acknowledged segments
    if (!_segments_outstanding.empty()) {
        std::set<TCPSegment, cmp>::iterator iter = _segments_outstanding.begin();
        while (iter != _segments_outstanding.end()) {
            if (ackno - iter->header().seqno >= static_cast<int32_t>(iter->length_in_sequence_space())) {
                _nBytes_inflight -= iter->length_in_sequence_space();
                _segments_outstanding.erase(iter++);
                _timer.start();
            } else
                iter++;
        }
    }
    //fill the window 
    fill_window();
    // when all outstanding data has been acknowledged, close the timer.
    if (_segments_outstanding.empty())
        _timer.close();
    return 1;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.tick(ms_since_last_tick))
        return;
    // timer has expired
    if (_segments_outstanding.empty())
        return;
    // retransmit the outstanding segment with the lowest sequence number
    std::set<TCPSegment, cmp>::iterator iter = _segments_outstanding.begin();
    _segments_out.push(*iter);
    if (_window_size) {
        _consecutive_retransmissions++;
        _timer._RTO *= 2;  // double the RTO, exponential backoff, it slows down retransmissions on lousy networks to
                           // avoid further gumming up the works
    }
    _timer.start();
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    assert(!seg.length_in_sequence_space());
    _segments_out.push(seg);
}
