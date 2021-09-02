#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    bool old_syn = syn, old_fin = fin;
    // 收到的TCP段不是syn而且系统还未接收到syn段
    if (!header.syn && !syn) {
        return false;
    }
    // re-assembler已经eof而且header中是fin
    if (_reassembler.eof() && header.fin) {
        return false;
    }
    if (!syn) {
        isn = header.seqno;
        syn = true;
    }
    uint64_t start = 0, size, end, seq_start, seq_size, seq_end, payload_end;
    if (ackno().has_value()) {
        start = unwrap(*ackno(), isn, checkpoint);
    }
    seq_start = unwrap(header.seqno, isn, checkpoint);
    seq_size = seg.length_in_sequence_space();
    if (seq_size == 0) {
        seq_size = 1;
    }
    seq_end = seq_start + seq_size - 1;
    if (header.syn && header.fin) {
        if (seq_size > 2) {
            seq_size -= 2;
        } else {
            seq_size = 1;
        }
    } else if (header.syn || header.fin) {
        if (seq_size > 1) {
            seq_size -= 1;
        } else {
            seq_size = 1;
        }
    }
    payload_end = seq_start + seq_size - 1;
    size = window_size() > 1 ? window_size() : 1;
    end = start + size - 1;
    bool inbound = (seq_start >= start && seq_start <= end) || (payload_end >= start && seq_end <= end);
    if (inbound) {
        _reassembler.push_substring(seg.payload().copy(), seq_start - 1, header.fin);
        checkpoint = _reassembler.first_unassembled_byte();
    }
    if (header.fin && !fin) {
        fin = true;
        if (header.syn && seg.length_in_sequence_space() == 2) {
            stream_out().end_input();
        }
    }
    bool fin_finished = false;
    if (syn) {
        fin_finished = fin && (_reassembler.unassembled_bytes() == 0);
        ack_number = wrap(_reassembler.first_unassembled_byte() + 1 + fin_finished, isn);
    }
    if (inbound || (header.fin && !old_fin) || (header.syn && !old_syn)) {
        return true;
    } else {
        return false;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn) {
        return nullopt;
    } else {
        return ack_number;
    }
}

size_t TCPReceiver::window_size() const {
    return stream_out().remaining_capacity();
}
