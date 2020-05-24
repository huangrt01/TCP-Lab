#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _stream(), _size(0), _capacity(capacity), _nwritten(0), _nread(0), _eof(0), _input_ended(0) {}

size_t ByteStream::write(const string &data) {
    size_t l = min(remaining_capacity(), data.size());
    for (size_t i = 0; i < l; i++) {
        _stream.emplace_back(data[i]);
    }
    _size += l;
    _nwritten += l;
    return l;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return string(_stream.begin(), _stream.begin() + min(len, _size));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t l = min(len, _size);
    for (size_t i = 0; i < l; i++)
        _stream.pop_front();
    _size -= l;
    _nread += l;
    if (buffer_empty() && input_ended())
        _eof = true;
}

void ByteStream::end_input() {
    _input_ended = 1;
    if (!_size)
        _eof = true;
}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return _eof; }

size_t ByteStream::bytes_written() const { return _nwritten; }

size_t ByteStream::bytes_read() const { return _nread; }

size_t ByteStream::remaining_capacity() const { return _capacity - _size; }
