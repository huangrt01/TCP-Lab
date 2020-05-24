#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _Unassembled(), _firstUnassembled(0), _nUnassembled(0), _capacity(capacity), _eof(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _eof = _eof | eof;
    if (data.empty() || index + data.size() <= _firstUnassembled) {
        if (_eof)
            _output.end_input();
        return;
    }
    if (index <= _firstUnassembled) {
        size_t wSize = _output.write(string(data.begin() + _firstUnassembled - index, data.end()));
        if (wSize == data.size() && eof)
            _output.end_input();
        _firstUnassembled += wSize;
        std::set<typeUnassembled>::iterator iter = _Unassembled.begin();
        // recursive method to make the coding style clear
        if (!empty()) {
            const std::string &tempData = (*iter).data;
            const size_t tempIndex = (*iter).index;
            _nUnassembled -= tempData.size();
            _Unassembled.erase(iter);
            push_substring(tempData, tempIndex, eof);
            if (empty() && _eof)
                _output.end_input();
        }
    } else {
        //不能直接影响_output的情形，储存的数据提前优化处理
        size_t resIndex = index;
        std::string resData = data;

        //向右合并
        std::set<typeUnassembled>::iterator iter = _Unassembled.lower_bound(typeUnassembled(index,data));
        if (iter!=_Unassembled.end()){
            if ((*iter).index < index + data.size()) {
                resData = string(data.begin(), data.end() - (index + data.size() - (*iter).index)) + (*iter).data;
                _nUnassembled -= (*iter).data.size();
                _Unassembled.erase(iter);
            }
        }
        // //向左合并
        _Unassembled.insert(typeUnassembled(resIndex, resData));
        _nUnassembled += resData.size();
    }
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _nUnassembled; }

bool StreamReassembler::empty() const { return _nUnassembled == 0; }
