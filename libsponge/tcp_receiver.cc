#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    bool received = 0;
    const TCPHeader &hdr = seg.header();
    // second syn or fin will be rejected
    if ((hdr.syn && _syn_received) || (!hdr.syn && !_syn_received))
        return false;
    if (_reassembler.eof() && hdr.fin)
        return false;
    if (hdr.syn) {
        _isn = hdr.seqno;
        _syn_received = true;
        _ackno = _isn;
        received = 1;
    }
    uint64_t win_start = 0;
    if (ackno().has_value()) {
        win_start = unwrap(*ackno(), _isn, 0);
    }
    uint64_t abs_seqno = unwrap(hdr.seqno, _isn, win_start);
    uint64_t seq_size = seg.length_in_sequence_space();
    if (!seq_size)
        seq_size = 1;

    uint64_t win_size = window_size();
    if (!win_size)
        win_size = 1;

    if (hdr.fin || hdr.syn) {
        _ackno++;
        received = 1;
    }

    if (abs_seqno + seq_size - 1 < win_start)
        return received;
    if (win_start + win_size - 1 < abs_seqno)
        return received;
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, hdr.fin);  //忽视syn，所以减1
    _ackno = wrap(_reassembler.first_unassembled_byte(), _isn) + 1;             //+1因为bytestream不给syn标号
    if (hdr.fin)
        _ackno++;
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_received)
        return nullopt;
    else
        return {_ackno};
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
