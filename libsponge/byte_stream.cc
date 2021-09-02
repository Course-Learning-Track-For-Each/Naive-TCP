#include "byte_stream.hh"
#include <algorithm>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t cap)
    : stream(), size(0), capacity(cap), num_read(0), num_write(0), input_end(0) {
}

size_t ByteStream::write(const string &data) {
    size_t len = min(remaining_capacity(), data.size());
    for (size_t i = 0; i < len; i ++) {
        stream.emplace_back(data[i]);
    }
    size += len;
    num_write += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return string(stream.begin(), stream.begin() + min(len, size));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = min(len, size);
    for (size_t i = 0; i < length; i ++) {
        stream.pop_front();
    }
    size -= length;
    num_read += length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const string result = peek_output(len);
    pop_output(len);
    return result;
}

void ByteStream::end_input() {
    this->input_end = 1;
}

bool ByteStream::input_ended() const {
    return input_end;
}

size_t ByteStream::buffer_size() const {
    return size;
}

bool ByteStream::buffer_empty() const {
    return size == 0;
}

// end when input ends and buffer is empty
bool ByteStream::eof() const {
    return input_ended() && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return num_write;
}

size_t ByteStream::bytes_read() const {
    return num_read;
}

size_t ByteStream::remaining_capacity() const {
    return capacity - size;
}
