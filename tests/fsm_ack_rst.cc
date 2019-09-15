#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

using namespace std;
using State = TCPTestHarness::State;

// in LISTEN, send ACK
static void ack_listen_test(const TCPConfig &cfg,
                            const WrappingInt32 seqno,
                            const WrappingInt32 ackno,
                            const int lineno) {
    try {
        TCPTestHarness test = TCPTestHarness::in_listen(cfg);

        // any ACK should result in a RST
        test.send_ack(seqno, ackno);

        test.execute(ExpectState{State::RESET});
        test.execute(ExpectOneSegment{}.with_rst(true), "test 3 failed: no RST after ACK in LISTEN");
    } catch (const exception &e) {
        throw runtime_error(string(e.what()) + " (ack_listen_test called from line " + to_string(lineno) + ")");
    }
}

// in SYN_SENT, send ACK and maybe RST
static void ack_rst_syn_sent_test(const TCPConfig &cfg,
                                  const WrappingInt32 base_seq,
                                  const WrappingInt32 seqno,
                                  const WrappingInt32 ackno,
                                  const bool send_rst,
                                  const int lineno) {
    try {
        TCPTestHarness test = TCPTestHarness::in_syn_sent(cfg, base_seq);

        // unacceptable ACKs should elicit a RST
        if (send_rst) {
            test.send_rst(seqno, ackno);
        } else {
            test.send_ack(seqno, ackno);
        }

        if (send_rst) {
            test.execute(ExpectNotInState{State::RESET});
            test.execute(ExpectNoSegment{}, "test 5 failed: bad ACK w/ RST should have been ignored in SYN_SENT");
        } else {
            test.execute(ExpectState{State::RESET});
            test.execute(ExpectOneSegment{}.with_rst(true).with_seqno(ackno),
                         "test 5 failed: problem with RST segment for bad ACK");
        }
    } catch (const exception &e) {
        throw runtime_error(string(e.what()) + " (ack_rst_syn_sent_test called from line " + to_string(lineno) + ")");
    }
}

int main() {
    try {
        TCPConfig cfg{};
        const WrappingInt32 base_seq(1 << 31);

        // test #1: in ESTABLISHED, send unacceptable segments and ACKs
        {
            cerr << "Test 1" << endl;
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, base_seq - 1, base_seq - 1);

            // acceptable ack---no response
            test_1.send_ack(base_seq, base_seq);

            test_1.execute(ExpectNoSegment{}, "test 1 failed: ACK after acceptable ACK");

            // ack in the past---no response
            test_1.send_ack(base_seq, base_seq - 1);

            test_1.execute(ExpectNoSegment{}, "test 1 failed: ACK after past ACK");

            // ack in the future---should get ACK back
            test_1.send_ack(base_seq, base_seq + 1);

            test_1.execute(ExpectOneSegment{}.with_ack(true).with_ackno(base_seq), "test 1 failed: bad ACK");

            // segment out of the window---should get an ACK
            test_1.send_byte(base_seq - 1, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{0}, "test 1 failed: seg queued on early seqno");

            test_1.execute(ExpectOneSegment{}.with_ack(true).with_ackno(base_seq), "test 1 failed: bad ACK");

            // segment out of the window---should get an ACK
            test_1.send_byte(base_seq + cfg.recv_capacity, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{0}, "test 1 failed: seg queued on late seqno");
            test_1.execute(ExpectOneSegment{}.with_ack(true).with_ackno(base_seq),
                           "test 1 failed: bad ACK on late seqno");

            // packet next byte in the window - ack should advance and data should be readable
            test_1.send_byte(base_seq, base_seq, 1);

            test_1.execute(ExpectData{}, "test 1 failed: pkt not processed on next seqno");

            test_1.execute(ExpectOneSegment{}.with_ack(true).with_ackno(base_seq + 1), "test 1 failed: bad ACK");

            // segment not in window with RST set --- should get nothing back
            test_1.send_rst(base_seq);
            test_1.send_rst(base_seq + cfg.recv_capacity + 1);
            test_1.execute(ExpectNoSegment{}, "test 1 failed: got a response to an out-of-window RST");

            test_1.send_rst(base_seq + 1);
            test_1.execute(ExpectState{State::RESET});
        }

        // test #2: in LISTEN, send RSTs
        {
            cerr << "Test 2" << endl;
            TCPTestHarness test_2 = TCPTestHarness::in_listen(cfg);

            // all RSTs should be ignored in LISTEN
            test_2.send_rst(base_seq);
            test_2.send_rst(base_seq - 1);
            test_2.send_rst(base_seq + cfg.recv_capacity);

            test_2.execute(ExpectNoSegment{}, "test 2 failed: RST was not ignored in LISTEN");
        }

        // test 3: ACKs in LISTEN
        cerr << "Test 3" << endl;
        ack_listen_test(cfg, base_seq, base_seq, __LINE__);
        ack_listen_test(cfg, base_seq - 1, base_seq, __LINE__);
        ack_listen_test(cfg, base_seq, base_seq - 1, __LINE__);
        ack_listen_test(cfg, base_seq - 1, base_seq, __LINE__);
        ack_listen_test(cfg, base_seq - 1, base_seq - 1, __LINE__);
        ack_listen_test(cfg, base_seq + cfg.recv_capacity, base_seq, __LINE__);
        ack_listen_test(cfg, base_seq, base_seq + cfg.recv_capacity, __LINE__);
        ack_listen_test(cfg, base_seq + cfg.recv_capacity, base_seq, __LINE__);
        ack_listen_test(cfg, base_seq + cfg.recv_capacity, base_seq + cfg.recv_capacity, __LINE__);

        // test 4: ACK and RST in SYN_SENT
        {
            cerr << "Test 4" << endl;
            TCPTestHarness test_4 = TCPTestHarness::in_syn_sent(cfg, base_seq);

            // data segments with acceptable ACKs should be ignored
            test_4.send_byte(base_seq, base_seq + 1, 1);

            test_4.execute(ExpectState{State::SYN_SENT});

            test_4.execute(ExpectNoSegment{}, "test 4 failed: bytes were not ignored in SYN_SENT");

            // RST segments without ACKs should be ignored
            test_4.send_rst(base_seq);
            test_4.send_rst(base_seq + 1);
            test_4.send_rst(base_seq - 1);

            test_4.execute(ExpectState{State::SYN_SENT});
            test_4.execute(ExpectNoSegment{}, "test 4 failed: RST without ACK was not ignored in SYN_SENT");

            // good ACK with RST should result in a RESET but no RST segment sent
            test_4.send_rst(base_seq, base_seq + 1);

            test_4.execute(ExpectState{State::RESET});
            test_4.execute(ExpectNoSegment{}, "test 4 failed: RST with good ackno should RESET the connection");
        }

        // test 5: ack/rst in SYN_SENT
        cerr << "Test 5" << endl;
        ack_rst_syn_sent_test(cfg, base_seq, base_seq, base_seq, false, __LINE__);
        ack_rst_syn_sent_test(cfg, base_seq, base_seq, base_seq + 2, false, __LINE__);
        ack_rst_syn_sent_test(cfg, base_seq, base_seq, base_seq, true, __LINE__);
        ack_rst_syn_sent_test(cfg, base_seq, base_seq, base_seq + 2, true, __LINE__);
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
