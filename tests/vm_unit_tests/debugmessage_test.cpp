#include <stdio.h>
#include <unistd.h>

#include <tuple>

#include "../../src/Debug/debugger.h"
#include "../../src/Utils/sockets.h"
#include "gtest/gtest.h"

class DebuggerMessage : public ::testing::Test {
   protected:
    const int fd;

    // For this tests, we use the channel as a one way communication from the
    // debugger to the testing class
    FileDescriptorChannel* channel;
    Debugger* debugger;

    DebuggerMessage() : fd(fileno(tmpfile())) {
        channel = new FileDescriptorChannel(fd);
        debugger = new Debugger(channel);
    }

    ~DebuggerMessage() override {
        close(fd);
        delete channel;
        delete debugger;
    }

    void SetUp() override {}

    void TearDown() override {}

    std::tuple<ssize_t, char*> readAnswer() {
        ssize_t valread;
        char* buffer = (char*)malloc(sizeof(char) * 1024);
        valread = channel->read(buffer, 1024);
        if (valread <= 0) {
            free(buffer);
            valread = 0;
            buffer = nullptr;
        }
        return {1, buffer};
    }
};

TEST_F(DebuggerMessage, InitiallyEmpty) {
    EXPECT_EQ(debugger->getDebugMessage(), nullptr)
        << "Debugger messages queue should start empty" << std::endl;
}

TEST_F(DebuggerMessage, ValidMessageIsQueued) {
    const uint8_t msg[] = {'0', '1', '\n'};  // run interrupt
    size_t len = 3;
    debugger->addDebugMessage(len, msg);
    EXPECT_NE(debugger->getDebugMessage(), nullptr)
        << "Valid message should be enqueued" << std::endl;
}

TEST_F(DebuggerMessage, InvalidMessagesAreNotQueued) {
    const char* msg = "INVALID";
    size_t len = 7;
    const short int amount_messages = 5;
    for (auto i = 0; i < amount_messages; i++) {
        debugger->addDebugMessage(len, reinterpret_cast<const uint8_t*>(msg));
    }
    EXPECT_EQ(debugger->getDebugMessage(), nullptr)
        << "Invalid messages should be ignored" << std::endl;
}

TEST_F(DebuggerMessage, MessagesAreRequestedInOrder) {
    const int amount_messages = 10;
    for (auto i = 0; i < amount_messages; i++) {
        uint8_t c = i + '0';
        const uint8_t msg[3] = {'0', c, '\n'};
        debugger->addDebugMessage(3, msg);
    }

    for (auto i = 0; i < amount_messages; i++) {
        uint8_t* msg = debugger->getDebugMessage();
        ASSERT_NE(msg, nullptr) << "A queued message was expected" << std::endl;
        ASSERT_EQ(*msg, i) << "Queued message does not match expected message"
                           << std::endl;
    }
}

// TEST_F(DebuggerMessage, InvalidCommandIsNotHandled) {
//   const uint8_t invalid_cmd[3] = {'0', '8',
//                                   '\n'};  // interrupt 8 does not exist
//   Module* unusedModule = nullptr;
//   RunningState* unusedState = nullptr;

//   debugger->addDebugMessage(3, invalid_cmd);
//   debugger->checkDebugMessages(unusedModule, unusedState);
//   const char* expected_msg = "COULD not parse interrupt data!\n"

//                              channel.write("COULD not parse interrupt
//                              data!\n");
// }

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
