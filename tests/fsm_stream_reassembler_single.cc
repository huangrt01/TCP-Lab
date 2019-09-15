#include "byte_stream.hh"
#include "fsm_stream_reassembler_harness.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ReassemblerTestHarness test{65000};

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"a", 0});

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("a"));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"a", 0}.with_eof(true));

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("a"));
            test.execute(AtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"", 0}.with_eof(true));

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(AtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"b", 0}.with_eof(true));

            test.execute(BytesAssembled(1));
            test.execute(BytesAvailable("b"));
            test.execute(AtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"", 0});

            test.execute(BytesAssembled(0));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
