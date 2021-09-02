#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), seg_set(), first_unassemble(0), num_bytes(0), _capacity(capacity), _eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

void StreamReassembler::push_substring(const string &data, const size_t index, const bool is_eof) {
    _eof = _eof | is_eof;
    if (data.size() > _capacity) {
        _eof = false;
    }
    if (data.empty() || index + data.size() <= first_unassemble) {
        if (_eof)
            _output.end_input();
        return;
    }
    size_t first_refuse = first_unassemble + (_capacity - _output.buffer_size());

    set<segment>::iterator iterator;
    size_t res_index = index;
    auto res_data = string(data);
    if (res_index < first_unassemble) {
        res_data = res_data.substr(first_unassemble - res_index);
        res_index = first_unassemble;
    }
    if (res_index + res_data.size() > first_refuse)
        res_data = res_data.substr(0, first_refuse - res_index);

    iterator = seg_set.lower_bound(segment(res_index,res_data));
    while(iterator != seg_set.begin()){
        //res_index > first_unassemble
        if(iterator == seg_set.end())
            iterator--;
        if (size_t delete_num = merge(res_index, res_data, iterator)) {  //返回值是删掉重合的bytes数
            num_bytes -= delete_num;
            if (iterator != seg_set.begin()){
                seg_set.erase(iterator--);
            } else{
                seg_set.erase(iterator);
                break;
            }
        }
        else {
            break;
        }
    }

    iterator = seg_set.lower_bound(segment(res_index, res_data));
    while(iterator != seg_set.end()){
        if (size_t delete_num = merge(res_index, res_data, iterator)) {  //返回值是删掉重合的bytes数
            seg_set.erase(iterator++);
            num_bytes -= delete_num;
        } else
            break;
    }

    if (res_index <= first_unassemble) {
        size_t write_size = _output.write(string(res_data.begin() + first_unassemble - res_index, res_data.end()));
        if (write_size == res_data.size() && _eof)
            _output.end_input();
        first_unassemble += write_size;
    } else {
        seg_set.insert(segment(res_index, res_data));
        num_bytes += res_data.size();
    }

    if (empty() && is_eof) {
        _output.end_input();
    }
    return;
}

int StreamReassembler::merge(size_t &index, string &data, set<segment>::iterator iter2) {
    // return value: 1:successfully merge; 0:fail to merge
    string data2 = (*iter2).data;
    size_t l2 = (*iter2).index, r2 = l2 + data2.size() - 1;
    size_t l1 = index, r1 = l1 + data.size() - 1;
    if (l2 > r1 + 1 || l1 > r2 + 1)
        return 0;
    index = min(l1, l2);
    size_t delete_num = data2.size();
    if (l1 <= l2) {
        if (r2 > r1)
            data += string(data2.begin() + r1 - l2 + 1, data2.end());
    } else {
        if (r1 > r2)
            data2 += string(data.begin() + r2 - l1 + 1, data.end());
        data.assign(data2);
    }
    return delete_num;
}

size_t StreamReassembler::unassembled_bytes() const { return num_bytes; }

bool StreamReassembler::empty() const { return num_bytes == 0; }
