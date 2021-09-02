#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return time_since_last_receive;
}

bool TCPConnection::active() const {
    bool is_shutdown = clean_shutdown || dirty_shutdown;
    return !is_shutdown && rst;
}

size_t TCPConnection::write(const string &data) {
    if (data.empty()) {
        return 0;
    }
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    fill_queue();
    check_tcp_end();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    time_since_last_receive += ms_since_last_tick;
    fill_queue();
    check_tcp_end();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    fill_queue();
    check_tcp_end();
}

void TCPConnection::connect() {
    _sender.fill_window();
    if (!rst) {
        rst = false;
    }
    fill_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::pop_tcp_segment(TCPSegment &seg) {
    seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value() && !rst) {
        seg.header().ackno = _receiver.ackno().value();
        seg.header().ack = true;
    }
    if (rst || (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)) {
        rst = true;
        seg.header().rst = true;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    } else {
        if (_receiver.window_size() < numeric_limits<uint16_t>::max()) {
            seg.header().win = _receiver.window_size();
        } else {
            seg.header().win = numeric_limits<uint16_t>::max();
        }
    }
}

void TCPConnection::check_tcp_end() {
    if (_receiver.stream_out().input_ended() && (!_sender.stream_in().eof()) && _sender.syn_sent()) {
        _linger_after_streams_finish = false;
    }
    if (_receiver.stream_out().eof() && _sender.stream_in().eof()
        && (unassembled_bytes()==0) && (bytes_in_flight()==0) && _sender.fin_sent()) {
        clean_shutdown |= !_linger_after_streams_finish;
        dirty_shutdown |= time_since_last_receive >= 10 * _cfg.rt_timeout;
    }
}

void TCPConnection::fill_queue() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg;
        pop_tcp_segment(seg);
        _segments_out.push(seg);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    time_since_last_receive = 0;
    bool send_receive = false, send_empty = false;
    if (seg.header().ack && _sender.syn_sent()) {
        send_receive = _sender.ack_received(seg.header().ackno, seg.header().win);
        if (!send_receive) {
            send_empty = true;
        } else {
            _sender.fill_window();
        }
    }
    bool receive = _receiver.segment_received(seg);
    if (!receive) {
        send_empty = true;
    }
    if (seg.header().syn && !_sender.syn_sent()) {
        connect();
        return;
    }
    if (seg.header().rst) {
        if (receive || (seg.header().ack && _sender.next_seqno() == seg.header().ackno)) {
            rst = true;
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            check_tcp_end();
        }
        return;
    }
    if (seg.header().fin) {
        if (!_sender.fin_sent()) {
            _sender.fill_window();
        }
        if (_sender.segments_out().empty()) {
            send_empty = true;
        }
    } else if (seg.length_in_sequence_space()) {
        send_empty = true;
    }
    if (send_empty) {
        if (_receiver.ackno().has_value() && _sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
    }
    fill_queue();
    check_tcp_end();
}