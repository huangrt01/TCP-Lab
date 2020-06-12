#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _ms_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _ms_since_last_segment_received = 0;
    if (seg.header().ack && _sender.syn_sent()) {
        if(!_sender.ack_received(seg.header().ackno, seg.header().win)){  // fsm_ack_rst_relaxed: ack in the future -> sent ack back
            send_ack_back();
            return;
        }
        while (!_sender.segments_out().empty()) {
            fill_queue();
            _sender.fill_window();
        }
    }
    // ignore out of window RST
    if (seg.header().rst) {
        _rst = 1;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }

    if(!_receiver.segment_received(seg)){
        send_ack_back();
        return;
    }
    
    if (seg.header().syn && !_sender.syn_sent()) {
        connect();
        return;
    }
    else if(seg.header().fin){
        if(!_sender.fin_sent())      //send FIN+ACK
            _sender.fill_window();
        if (_sender.segments_out().empty())     //send ACK
            _sender.send_empty_segment();  
        fill_queue();
    }
    else if(seg.length_in_sequence_space()){
        send_ack_back();
        return;
    }
    test_end();
}

bool TCPConnection::active() const { 
    return (!_clean_shutdown) && (!_unclean_shutdown) && (!_rst); 
}

size_t TCPConnection::write(const string &data) {
    if(data.size()==0) return 0;
    size_t size=_sender.stream_in().write(data);
    _sender.fill_window();
    if (!_sender.segments_out().empty()) 
        fill_queue();
    test_end();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick); 
    _ms_since_last_segment_received+=ms_since_last_tick;
    fill_queue();
    test_end();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    fill_queue();
    test_end();
}

void TCPConnection::connect() {
    _sender.fill_window();
    if(!_rst) _rst=0;
    fill_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // send a RST segment to the peer
            _rst=1;
            _sender.send_empty_segment();
            fill_queue();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::popTCPSegment(TCPSegment &seg){
    // send a segment
    seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value()) {
        seg.header().ackno = _receiver.ackno().value();
        seg.header().ack = true;
    }

    if(_rst || (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)) {
        // send RST
        _rst=1;
        seg.header().rst = true;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    } else {
        // TCPReceiver wants to advertise a window size
        // that’s bigger than will fit in the TCPSegment::header().win field
        if (_receiver.window_size() < numeric_limits<uint16_t>::max())
            seg.header().win = _receiver.window_size();
        else
            seg.header().win = numeric_limits<uint16_t>::max();
    }
}

//send ack back
void TCPConnection::send_ack_back(){
    //if the ackno is missing, don't send back an ACK.
    if(!_receiver.ackno().has_value()) return;

    //sometimes the TCPSender will send a segment in ack_received()
    if(_sender.segments_out().empty())
        _sender.send_empty_segment();
    fill_queue();
}

//test the end of TCP connection
void TCPConnection::test_end(){
    if (_receiver.stream_out().input_ended() && (!_sender.stream_in().eof()) && _sender.syn_sent())
        _linger_after_streams_finish = false;
    if(_receiver.stream_out().eof() && _sender.stream_in().eof() && (unassembled_bytes()==0) && (bytes_in_flight()==0) && _sender.fin_sent()){
        //bytes_in_flight==0 => state: FIN_ACKED
        _clean_shutdown |= (!_linger_after_streams_finish);
        _unclean_shutdown |= (_ms_since_last_segment_received >= 10 * _cfg.rt_timeout);
    }
}

//fill queue from _sender.segments_out() to _segments_out
void TCPConnection::fill_queue(){
    while (!_sender.segments_out().empty()) {
        TCPSegment seg;
        popTCPSegment(seg);
        _segments_out.push(seg);
    }
}
