// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/service/lm/lm.h"

namespace Service {
namespace LM {

class Logger final : public ServiceFramework<Logger> {
public:
    Logger() : ServiceFramework("Logger") {
        static const FunctionInfo functions[] = {
            {0x00000000, &Logger::Log, "Log"},
        };
        RegisterHandlers(functions);
    }

    ~Logger() = default;

private:
    struct MessageHeader {
        enum Flags : u32_le {
            IsHead = 1,
            IsTail = 2,
        };
        enum Severity : u32_le {
            Trace,
            Info,
            Warning,
            Error,
            Critical,
        };

        u64_le pid;
        u64_le threadContext;
        union {
            BitField<0, 16, Flags> flags;
            BitField<16, 8, Severity> severity;
            BitField<24, 8, u32_le> verbosity;
        };
        u32_le payload_size;

        bool IsHeadLog() const {
            return flags & Flags::IsHead;
        }
        bool IsTailLog() const {
            return flags & Flags::IsTail;
        }
    };
    static_assert(sizeof(MessageHeader) == 0x18, "MessageHeader is incorrect size");

    /// Log field type
    enum class Field : u8 {
        Skip = 1,
        Message = 2,
        Line = 3,
        Filename = 4,
        Function = 5,
        Module = 6,
        Thread = 7,
    };

    /**
     * LM::Log service function
     *  Inputs:
     *      0: 0x00000000
     *  Outputs:
     *      0: ResultCode
     */
    void Log(Kernel::HLERequestContext& ctx) {
        // This function only succeeds - Get that out of the way
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        // Read MessageHeader, despite not doing anything with it right now
        MessageHeader header{};
        VAddr addr{ctx.BufferDescriptorX()[0].Address()};
        const VAddr end_addr{addr + ctx.BufferDescriptorX()[0].size};
        Memory::ReadBlock(addr, &header, sizeof(MessageHeader));
        addr += sizeof(MessageHeader);

        if (header.IsHeadLog()) {
            log_stream.str("");
            log_stream.clear();
        }

        // Parse out log metadata
        u32 line{};
        std::string message, filename, function;
        while (addr < end_addr) {
            const Field field{static_cast<Field>(Memory::Read8(addr++))};
            const size_t length{Memory::Read8(addr++)};

            if (static_cast<Field>(Memory::Read8(addr)) == Field::Skip) {
                ++addr;
            }

            switch (field) {
            case Field::Message:
                message = Memory::ReadCString(addr, length);
                break;
            case Field::Line:
                line = Memory::Read32(addr);
                break;
            case Field::Filename:
                filename = Memory::ReadCString(addr, length);
                break;
            case Field::Function:
                function = Memory::ReadCString(addr, length);
                break;
            }

            addr += length;
        }

        // Empty log - nothing to do here
        if (log_stream.str().empty() && message.empty()) {
            return;
        }

        // Format a nicely printable string out of the log metadata
        if (!filename.empty()) {
            log_stream << filename << ':';
        }
        if (!function.empty()) {
            log_stream << function << ':';
        }
        if (line) {
            log_stream << std::to_string(line) << ':';
        }
        if (log_stream.str().length() > 0 && log_stream.str().back() == ':') {
            log_stream << ' ';
        }
        log_stream << message;

        if (header.IsTailLog()) {
            switch (header.severity) {
            case MessageHeader::Severity::Trace:
                LOG_TRACE(Debug_Emulated, "%s", log_stream.str().c_str());
                break;
            case MessageHeader::Severity::Info:
                LOG_INFO(Debug_Emulated, "%s", log_stream.str().c_str());
                break;
            case MessageHeader::Severity::Warning:
                LOG_WARNING(Debug_Emulated, "%s", log_stream.str().c_str());
                break;
            case MessageHeader::Severity::Error:
                LOG_ERROR(Debug_Emulated, "%s", log_stream.str().c_str());
                break;
            case MessageHeader::Severity::Critical:
                LOG_CRITICAL(Debug_Emulated, "%s", log_stream.str().c_str());
                break;
            }
        }
    }

    std::ostringstream log_stream;
};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<LM>()->InstallAsService(service_manager);
}

/**
 * LM::Initialize service function
 *  Inputs:
 *      0: 0x00000000
 *  Outputs:
 *      0: ResultCode
 */
void LM::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<Logger>();

    LOG_DEBUG(Service_LM, "called");
}

LM::LM() : ServiceFramework("lm") {
    static const FunctionInfo functions[] = {
        {0x00000000, &LM::Initialize, "Initialize"},
    };
    RegisterHandlers(functions);
}

} // namespace LM
} // namespace Service
