#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : deq(), 
                                                capacity_(capacity), 
                                                written_(0), 
                                                read_(0), 
                                                end_(false) {  }

size_t ByteStream::write(const string &data) {
    if (end_) {
        std::cout << "buffer is full, can not write any more byte" << std::endl;
        return 0;
    }
    size_t remain = min(data.size(), capacity_ - deq.size()); // written_ == deq.size()
    for (size_t i = 0; i < remain; i++) {
        deq.push_back(data[i]);
    }
    written_ += remain;
    // 为什么不需要维护？
    // if (written_ == capacity_)
    //     end_ = true;
    return remain;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > deq.size())
        length = deq.size();
    string str(deq.begin(), deq.begin() + length);
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = min(len, deq.size());
    read_ += length;
    for (size_t i = 0; i < length; i++)
        deq.pop_front();
}

void ByteStream::end_input() {
    end_ = true;
}

bool ByteStream::input_ended() const {
    return end_;
}

size_t ByteStream::buffer_size() const {
    return deq.size();
}

bool ByteStream::buffer_empty() const {
    return deq.empty();
}

bool ByteStream::eof() const {
    return end_ /* 表示已经写满了 */ && deq.empty();
}

size_t ByteStream::bytes_written() const {
    return written_;
}

size_t ByteStream::bytes_read() const {
    return read_;
}

size_t ByteStream::remaining_capacity() const {
    return capacity_ - deq.size();
}
