#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    const bool old_syn_received = _syn_received, old_fin_received = _fin_received;
    const TCPHeader &hdr = seg.header();

    if (!hdr.syn && !_syn_received) {
        return false;
    }
    if (_reassembler.eof() && hdr.fin) {
        return false;
    }

    if (!_syn_received) {
        _isn = hdr.seqno;
        _syn_received = true;
    }
    uint64_t win_start = unwrap(*ackno(), _isn, _checkpoint);
    uint64_t win_size = window_size() ? window_size() : 1;
    uint64_t win_end = win_start + win_size - 1;

    uint64_t seq_start = unwrap(hdr.seqno, _isn, _checkpoint);
    uint64_t seq_size = seg.length_in_sequence_space();
    seq_size = (seq_size) ? seq_size : 1;
    uint64_t seq_end = seq_start + seq_size - 1;

    uint64_t payload_size = seg.payload().str().size();
    payload_size = (payload_size) ? payload_size : 1;
    uint64_t payload_end = seq_start + payload_size - 1;

    bool inbound = (seq_start >= win_start && seq_start <= win_end) || (payload_end >= win_start && seq_end <= win_end);
    if (inbound) {
        _reassembler.push_substring(seg.payload().copy(), seq_start - 1, hdr.fin);  //忽视syn，所以减1
        _checkpoint = _reassembler.first_unassembled_byte();
    }

    if (hdr.fin && !_fin_received) {
        _fin_received = 1;
        // if flags = SF and payload_size = 0, we need to end_input() the stream manually
        if (hdr.syn && seg.length_in_sequence_space() == 2) {
            stream_out().end_input();
        }
    }

    bool fin_finished = _fin_received && (_reassembler.unassembled_bytes() == 0);
    _ackno = wrap(_reassembler.first_unassembled_byte() + 1 + fin_finished, _isn);  //+1因为bytestream不给syn标号

    // second syn or fin will be rejected
    if ((inbound) || (hdr.fin && !old_fin_received) || (hdr.syn && !old_syn_received)) {
        return true;
    }
    return false;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_received) {
        return nullopt;
    } else {
        return {_ackno};
    }
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
