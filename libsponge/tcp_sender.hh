#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <set>

//! \brief The "sender" part of a TCP implementation.
class TCPRetransmissionTimer {
  public:
    unsigned int init_rto;
    unsigned int rto;
    unsigned int to;
    bool open;
    TCPRetransmissionTimer(const uint16_t time_out)
        : init_rto(time_out), rto(time_out), to(0), open(true) {

    }

    bool is_open() {
        return open;
    }

    // start and close the timer
    void start() {
        open = 1;
        to = 0;
    }

    void close() {
        open = 0;
        to = 0;
    }

    bool tick(size_t &time) {
        if (!is_open()) {
            return false;
        }
        if (time > rto - to) {
            time -= (rto - to);
            to = rto;
        } else {
            to += time;
            time = 0;
        }
        if (to >= rto) {
            to = 0;
            return true;
        }
        return false;
    }
};

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    std::queue<TCPSegment> _segments_outstanding;

    uint64_t bytes_inflight;
    uint64_t receive_ack_number;

    uint16_t window_size;

    TCPRetransmissionTimer timer;

    //! retransmission timer for the connection
    unsigned int consecutive_retransmission;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    bool syn_send, fin_send;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();
    void send_non_empty_segment(TCPSegment &segment);

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    uint64_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    bool syn_sent() const {
        return syn_send;
    }

    bool fin_sent() const {
        return fin_send;
    }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
