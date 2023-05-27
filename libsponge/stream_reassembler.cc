#include "stream_reassembler.hh"

#include <cassert>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
                                    _nextAssembled_idx(0),
                                    unreassmbler_str(),
                                    byteSizeInunreassmble(0),
                                    isEof_idx(-1),
                                    _output(capacity), 
                                    _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 1.判断unreassmbler_str中是否存在这个substring
    // 处理头部
    auto p_iter = unreassmbler_str.upper_bound(index);
    if (p_iter != unreassmbler_str.begin())
        p_iter--;
    // 1.1 获取当前sunstring的新起始位置
    size_t new_idx = index;

    if (p_iter != unreassmbler_str.end() && index >= p_iter->first) { // 前面有substring
        const size_t up_idx = p_iter->first;
        if (index < up_idx + p_iter->second.size()) // 与前面的substring出现了重叠
            new_idx = up_idx + p_iter->second.size();
    } else if (index < _nextAssembled_idx) // 前面没有substring
        new_idx = _nextAssembled_idx;

    const size_t data_start_position = new_idx - index;
    ssize_t data_size = data.size() - data_start_position; // 当前substring保留的长度

    // 处理尾部
    p_iter = unreassmbler_str.lower_bound(new_idx);
    while (p_iter != unreassmbler_str.end() && new_idx <= p_iter->first) {
        const size_t data_end_position = new_idx + data_size;
        if (p_iter->first < data_end_position) { // 存在重叠区域
            if (data_end_position < p_iter->first + p_iter->second.size()) { // 部分重叠
                data_size = p_iter->first - new_idx;
                break;
            } else { // 全部重叠
                byteSizeInunreassmble -= p_iter->second.size();
                p_iter = unreassmbler_str.erase(p_iter); // 重新iterator
                continue;
            }
        } else 
            break;
    }

    // 超出容量检测
    size_t first_unacceptable_idx = _nextAssembled_idx + _capacity - _output.buffer_size();
    if (first_unacceptable_idx <= index)
        return;


    // 2. 判断能不能把这个substring直接插入_output中
    if (data_size > 0) {
        const string new_data = data.substr(data_start_position, data_size);
        if (new_idx == _nextAssembled_idx) { // 可以直接写入_output
            const size_t write_byte = _output.write(new_data);
            _nextAssembled_idx += write_byte;
            if (write_byte < new_data.size()) { // 没有全部写完
                const string data_store = new_data.substr(write_byte, new_data.size() - write_byte);
                byteSizeInunreassmble += data_store.size();
                unreassmbler_str.insert(make_pair(_nextAssembled_idx, std::move(data_store)));
            }
        } else { // 保存在unreassmbler_str中
            const string data_store = new_data.substr(0, new_data.size());
            byteSizeInunreassmble += data_store.size();
            unreassmbler_str.insert(make_pair(new_idx, std::move(data_store)));
        }
    } 

    // 3. 通过遍历把unreassmbler_str中的substring插入到_output中
    for (auto iter = unreassmbler_str.begin(); iter != unreassmbler_str.end(); ) {
        assert(_nextAssembled_idx <= iter->first);
        if (iter->first == _nextAssembled_idx) {
            const size_t write_num = _output.write(iter->second);
            _nextAssembled_idx += write_num;
            if (write_num < iter->second.size()) { // 写满了
                byteSizeInunreassmble += iter->second.size() - write_num;
                unreassmbler_str.insert(make_pair(_nextAssembled_idx, std::move(iter->second.substr(write_num))));
                byteSizeInunreassmble -= iter->second.size();
                unreassmbler_str.erase(iter);
                break;
            }
            byteSizeInunreassmble -= iter->second.size();
            iter = unreassmbler_str.erase(iter);
        } else
            break;
    }

    if (eof)
        isEof_idx = index + data.size();
    if (isEof_idx <= _nextAssembled_idx)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    return byteSizeInunreassmble;
}

bool StreamReassembler::empty() const {
    return byteSizeInunreassmble == 0;
}
