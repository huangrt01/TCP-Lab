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
    _eof = _eof | eof;
    if (data.size() > _capacity) {
        _eof = 0;
    }
    if (data.empty() || index + data.size() <= _firstUnassembled) {
        if (_eof)
            _output.end_input();
        return;
    }
    size_t firstUnacceptable = _firstUnassembled + (_capacity - _output.buffer_size());

    //不能直接影响_output的情形，储存的数据提前优化处理
    std::set<typeUnassembled>::iterator iter2;
    size_t resIndex = index;
    auto resData = std::string(data);
    if (resIndex < _firstUnassembled) {
        resData = resData.substr(_firstUnassembled - resIndex);
        resIndex = _firstUnassembled;
    }
    if (resIndex + resData.size() > firstUnacceptable)
        resData = resData.substr(0, firstUnacceptable - resIndex);
    
    //           | resData |
    //        <---|iter|
    iter2=_Unassembled.lower_bound(typeUnassembled(resIndex,resData));
    while(iter2!=_Unassembled.begin()){  
        //resIndex > _firstUnassembled
        if(iter2==_Unassembled.end())
            iter2--;
        if (size_t deleteNum = merge_substring(resIndex, resData, iter2)) {  //返回值是删掉重合的bytes数
            _nUnassembled -= deleteNum;
            if(iter2 !=_Unassembled.begin()){
                _Unassembled.erase(iter2--);
            }
            else{
                _Unassembled.erase(iter2);
                break;
            }
        } 
        else
            break;
    }

    //         ｜resData |
    //          | iter2 ... | --->
    iter2 = _Unassembled.lower_bound(typeUnassembled(resIndex, resData));
    while(iter2 != _Unassembled.end()){
        if (size_t deleteNum = merge_substring(resIndex, resData, iter2)) {  //返回值是删掉重合的bytes数
            _Unassembled.erase(iter2++);
            _nUnassembled -= deleteNum;
        } else
            break;
    }

    if (resIndex <= _firstUnassembled) {
        size_t wSize = _output.write(string(resData.begin() + _firstUnassembled - resIndex, resData.end()));
        if (wSize == resData.size() && eof)
            _output.end_input();
        _firstUnassembled += wSize;
    } else {
        _Unassembled.insert(typeUnassembled(resIndex, resData));
        _nUnassembled += resData.size();
    }

    if (empty() && _eof) {
        _output.end_input();
    }
    return;
}
int StreamReassembler::merge_substring(size_t &index, std::string &data, std::set<typeUnassembled>::iterator iter2) {
    // return value: 1:successfully merge; 0:fail to merge
    std::string data2 = (*iter2).data;
    size_t l2 = (*iter2).index, r2 = l2 + data2.size() - 1;
    size_t l1 = index, r1 = l1 + data.size() - 1;
    if (l2 > r1 + 1 || l1 > r2 + 1)
        return 0;
    index = min(l1, l2);
    size_t deleteNum = data2.size();
    if (l1 <= l2) {
        if (r2 > r1)
            data += std::string(data2.begin() + r1 - l2 + 1, data2.end());
    } else {
        if (r1 > r2)
            data2 += std::string(data.begin() + r2 - l1 + 1, data.end());
        data.assign(data2);
    }
    return deleteNum;
}

size_t StreamReassembler::unassembled_bytes() const { return _nUnassembled; }

bool StreamReassembler::empty() const { return _nUnassembled == 0; }

// std::pair<std::set<typeUnassembled>::iterator,bool> ret;
// ret=_Unassembled.insert(typeUnassembled(index,data));
// _nUnassembled+=data.size();
// std::set<typeUnassembled>::iterator iter1=ret.first;, 太坑了！返回的不是它本身！！！！！！！！
