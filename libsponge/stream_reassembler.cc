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

void StreamReassembler::push_substring(const std::string &data, const size_t index, const bool eof) {
    if (data.empty() || index + data.size() <= _firstUnassembled) {
        _eof = _eof | eof;
        if (empty() && _eof) {
            _output.end_input();
        }
        return;
    }
    size_t firstUnacceptable = _firstUnassembled + (_capacity - _output.buffer_size());

    //不能直接影响_output的情形，储存的数据提前优化处理
    size_t resIndex = index;
    size_t beginIndex = 0;
    size_t endIndex = data.size();
    if (index < _firstUnassembled) {
        resIndex = _firstUnassembled;
        beginIndex = _firstUnassembled - index;
    }
    if (index + data.size() >= firstUnacceptable) {
        endIndex = firstUnacceptable - index;
    }
    auto resData = std::string(data.begin() + beginIndex, data.begin() + endIndex);
    //           | resData |
    //        <---|iter|
    auto iter = _Unassembled.lower_bound(typeUnassembled(resIndex, ""));
    while (iter != _Unassembled.begin()) {
        // resIndex > _firstUnassembled
        if (iter == _Unassembled.end()) {
            iter--;
        }
        if (size_t deleteNum =
                merge_substring(resIndex, resData, (*iter).index, (*iter).data)) {  //返回值是删掉重合的bytes数
            _nUnassembled -= deleteNum;
            if (iter != _Unassembled.begin()) {
                _Unassembled.erase(iter--);
            } else {
                _Unassembled.erase(iter);
                break;
            }
        } else {
            break;
        }
    }

    //         ｜resData |
    //          | iter ... | --->
    iter = _Unassembled.lower_bound(typeUnassembled(resIndex, ""));
    while (iter != _Unassembled.end()) {
        if (size_t deleteNum = merge_substring(resIndex, resData, (*iter).index, (*iter).data)) {
            _Unassembled.erase(iter++);
            _nUnassembled -= deleteNum;
        } else {
            break;
        }
    }

    if (resIndex == _firstUnassembled) {
        size_t wSize = _output.write(resData);
        if ((wSize == resData.size()) && eof) {
            _eof = true;
            _output.end_input();
        }
        _firstUnassembled += wSize;
    }
    if (resData.empty()) {
        _eof = _eof | eof;
    } else if (resIndex > _firstUnassembled) {
        _eof = _eof | eof;
        _Unassembled.insert(typeUnassembled(resIndex, resData));
        _nUnassembled += resData.size();
    }

    if (empty() && _eof) {
        _output.end_input();
    }
    return;
}
int StreamReassembler::merge_substring(size_t &index, std::string &data, size_t index2, const std::string &data2) {
    // return value:
    // > 0: successfully merge, return the overlapped size
    // 0: fail to merge
    size_t l1 = index, r1 = l1 + data.size() - 1;
    size_t l2 = index2, r2 = l2 + data2.size() - 1;
    if (l2 > r1 + 1 || l1 > r2 + 1) {
        return 0;
    }
    index = min(l1, l2);
    size_t deleteNum = data2.size();
    if (l1 <= l2) {
        if (r2 > r1) {
            data += std::string(data2.begin() + r1 - l2 + 1, data2.end());
        }
    } else {
        if (r1 > r2) {
            data = data2 + std::string(data.begin() + r2 - l1 + 1, data.end());
        } else {
            data = data2;
        }
    }
    return deleteNum;
}

size_t StreamReassembler::unassembled_bytes() const { return _nUnassembled; }

bool StreamReassembler::empty() const { return _nUnassembled == 0; }
