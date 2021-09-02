#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()})),
    _segments_out{},
    _segments_outstanding{},
    bytes_inflight(0),
    receive_ack_number(0),
    window_size(1),
    timer(retx_timeout),
    consecutive_retransmission{0},
    _stream(capacity),
    _next_seqno(0),
    syn_send(false),
    fin_send(false) {
    // pass
}

uint64_t TCPSender::bytes_in_flight() const {
    return bytes_inflight;
}

void TCPSender::fill_window() {
    TCPSegment seg;
    if (_next_seqno == 0) {
        seg.header().syn = 1;
        syn_send = true;
        send_non_empty_segment(seg);
        return;
    } else if (_next_seqno == bytes_inflight) {
        return;
    }
    uint64_t win = window_size;
    if (window_size == 0) {
        win = 1;
    }
    uint64_t remain;
    while ((remain = win + (receive_ack_number - _next_seqno))) {
        TCPSegment seg2;
        if (_stream.eof() && !fin_send) {
            seg2.header().fin = 1;
            fin_send = true;
            send_non_empty_segment(seg2);
            return;
        } else if (_stream.eof()) {
            return;
        } else {
            size_t size = min(size_t(remain), TCPConfig::MAX_PAYLOAD_SIZE);
            seg2.payload() = Buffer(_stream.read(size));
            if (seg2.length_in_sequence_space() < win && _stream.eof()) {
                seg2.header().fin = 1;
                fin_send = true;
            }
            if (seg2.length_in_sequence_space() == 0) {
                return;
            }
            send_non_empty_segment(seg2);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
bool TCPSender::ack_received(const WrappingInt32 ack_no, const uint16_t win_size) {
    uint64_t ack = unwrap(ack_no, _isn, receive_ack_number);
    if (ack - _next_seqno > 0) {
        return false;
    }
    window_size = win_size;
    if (ack - receive_ack_number <= 0) {
        return true;
    }
    receive_ack_number = ack;
    timer.rto = timer.init_rto;
    consecutive_retransmission = 0;
    TCPSegment seg;
    while (!_segments_outstanding.empty()) {
        seg = _segments_outstanding.front();
        if (ack_no - seg.header().seqno >= int32_t(seg.length_in_sequence_space())) {
            bytes_inflight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        } else {
            break;
        }
    }
    fill_window();
    if (!_segments_outstanding.empty()) {
        timer.start();
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    size_t time_left = ms_since_last_tick;
    if (timer.tick(time_left)) {
        if (!_segments_outstanding.empty()) {
            _segments_out.push(_segments_outstanding.front());
            if (window_size) {
                consecutive_retransmission += 1;
                timer.rto *= 2;
            }
            if (!timer.is_open()) {
                timer.start();
            }
            if (syn_sent() && (_next_seqno == bytes_inflight)) {
                if (timer.rto < timer.init_rto) {
                    timer.rto = timer.init_rto;
                }
            }
        }
        if (_segments_outstanding.empty()) {
            timer.close();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return consecutive_retransmission;
}

void TCPSender::send_non_empty_segment(TCPSegment &segment) {
    segment.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += segment.length_in_sequence_space();
    bytes_inflight += segment.length_in_sequence_space();
    _segments_out.push(segment);
    _segments_outstanding.push(segment);
    if (!timer.is_open()) {
        timer.start();
    }
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
