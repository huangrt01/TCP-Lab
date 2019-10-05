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
            ReassemblerTestHarness test{2};

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAssembled(2));
            test.execute(BytesAvailable("ab"));

            test.execute(SubmitSegment{"cd", 2});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("cd"));

            test.execute(SubmitSegment{"ef", 4});
            test.execute(BytesAssembled(6));
            test.execute(BytesAvailable("ef"));
        }

        {
            ReassemblerTestHarness test{2};

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAssembled(2));

            test.execute(SubmitSegment{"cd", 2});
            test.execute(BytesAssembled(2));

            test.execute(BytesAvailable("ab"));
            test.execute(BytesAssembled(2));

            test.execute(SubmitSegment{"cd", 2});
            test.execute(BytesAssembled(4));

            test.execute(BytesAvailable("cd"));
        }

        {
            ReassemblerTestHarness test{1};

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAssembled(1));

            test.execute(SubmitSegment{"ab", 0});
            test.execute(BytesAssembled(1));

            test.execute(BytesAvailable("a"));
            test.execute(BytesAssembled(1));

            test.execute(SubmitSegment{"abc", 0});
            test.execute(BytesAssembled(2));

            test.execute(BytesAvailable("b"));
            test.execute(BytesAssembled(2));
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
