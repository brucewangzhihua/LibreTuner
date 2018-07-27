#ifndef J2534_H
#define J2534_H

#include <memory>
#include <string>

#include "datalink.h"

#include <Windows.h>

namespace j2534 {

struct Info {
    std::string name;
    // Supported protocols
    DataLinkProtocol protocols;
    // DLL path
    std::string functionLibrary;
};



// J2534 API

struct PASSTHRU_MSG {
    uint32_t ProtocolID; /* vehicle network protocol */
    uint32_t RxStatus; /* receive message status */
    uint32_t TxFlags; /* transmit message flags */
    uint32_t Timestamp; /* receive message timestamp(in microseconds) */
    uint32_t DataSize; /* byte size of message payload in the Data array */
    uint32_t ExtraDataIndex; /* start of extra data(i.e. CRC, checksum, etc) in Data array */
    unsigned char Data[4128]; /* message payload or data */
};

using PassThruOpen_t = long (*) (void*, uint32_t*);
using PassThruClose_t = long (*) (uint32_t);
using PassThruConnect_t = long (*) (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t*);
using PassThruDisconnect_t = long (*) (uint32_t);
using PassThruReadMsgs_t = long (*) (uint32_t, PASSTHRU_MSG*, uint32_t*, uint32_t);
using PassThruWriteMsgs_t = long (*) (uint32_t, PASSTHRU_MSG*, uint32_t*, uint32_t);
using PassThruStartPeriodicMsg_t = long (*) (uint32_t, PASSTHRU_MSG*, uint32_t*, uint32_t);
using PassThruStopPeriodicMsg_t = long (*) (uint32_t, uint32_t);
using PassThruStartMsgFilter_t = long (*) (uint32_t, uint32_t, PASSTHRU_MSG*, PASSTHRU_MSG*, PASSTHRU_MSG*, uint32_t*);
using PassThruStopMsgFilter_t = long (*) (uint32_t, uint32_t);
using PassThruSetProgrammingVoltage_t = long (*) (uint32_t, uint32_t);
using PassThruReadVersion_t = long (*) (char*, char*, char*);
using PassThruGetLastError_t = long (*) (char*);
using PassThruIoctl_t = long (*) (uint32_t, uint32_t, void*, void*);


class J2534;
using J2534Ptr = std::shared_ptr<J2534>;

enum class Protocol {
    None = 0,
    J1850VPW = 1,
    J1850PWM = 2,
    ISO9141 = 3,
    ISO14230 = 4,
    CAN = 5,
    ISO15765 = 6,
    SCI_A_Engine = 7,
    SCI_A_Trans = 8,
    SCI_B_Engine = 9,
    SCI_B_Trans = 10,
};

enum class Ioctl {
    GetConfig = 0x01,
    SetConfig = 0x02,
    ReadVbatt = 0x03,
    FiveBaudInit = 0x04,
    FastInit = 0x05,
    ClearTxBuffer = 0x07,
    ClearRxBuffer = 0x08,
    ClearPeriodicMsgs = 0x09,
    ClearMsgFilters = 0x0A,
    ClearFunctMsgLookupTable = 0x0B,
    AddToFunctMsgLookupTable = 0x0C,
    DeleteFromFunctMsgLookupTable = 0x0D,
    ReadProgVoltage = 0x0E,
    // J2534-2 SW_CAN
    SwCanHs = 0x8000,
    SwCanNs = 0x8001,
    SetPollResponse = 0x8002,
    BecomeMaster = 0x8003,
};

class Channel {
public:
    // This should only be constructed internally.
    // Use J2534Device::connect
    Channel(const J2534Ptr &j2534, uint32_t channel) : j2534_(j2534), channel_(channel) {}
    // Creates an invalid device
    Channel() = default;

    ~Channel();

    Channel(const Channel&) = delete;
    Channel(Channel&& chann);

    /* Disconnects the channel from the j2534 device. The object
     * is in an invalid state after calling this method */
    void disconnect();

    bool valid() const { return !!j2534_; }

private:
    J2534Ptr j2534_;
    uint32_t channel_;
};

class Device {
public:
    // This object should never be contructed by the client. Use
    // J2534::open instaed
    Device(const J2534Ptr &j2534, uint32_t device);
    // Creates an invalid device
    Device() = default;

    ~Device();

    // closes the J2534 device and all channels
    void close();

    /* Establishes a logical communication channel with the vehicle
     * network (via the PassThru device) using the specified network
     * layer protocol and selected protocol options. */
    Channel connect(Protocol protocol, uint32_t flags = 0, uint32_t baudrate = 500000);

    Device(const Device&) = delete;
    Device(Device&& dev);

    bool valid() const { return !!j2534_; }

private:
    J2534Ptr j2534_;
    uint32_t device_;
};

class J2534 : public std::enable_shared_from_this<J2534>
{
public:
    // Initializes the interface by loading the DLL. May throw an exception
    void init();

    // Returns true if the interface's library has been loaded
    bool initialized() const { return loaded_; }

    // Opens a J2534 device. If no device is connected, returns an invalid device
    // check J2534Device::valid()
    Device open(char *port = nullptr);

    // Closes a  J2534 device
    void close(uint32_t device);

    // see Device::connect
    Channel connect(uint32_t device, Protocol protocol, uint32_t flags, uint32_t baudrate);

    // Disconnects a logical communication channel
    void disconnect(uint32_t channel);

    std::string lastError();

    std::string name() const { return info_.name; }

    // Returns the protocols supported by the J2534 interface
    DataLinkProtocol protocols() const { return info_.protocols; }

    // Creates a J2534 interface. Must be initialized with init() before use.
    static J2534Ptr create(Info &&info);

    J2534(Info &&info) : info_(std::move(info)) {}

    ~J2534();

private:
    Info info_;

    HINSTANCE hDll_ = nullptr;

    bool loaded_ = false;

    // Loads the dll
    void load();

    void *getProc(const char *proc);

    PassThruOpen_t PassThruOpen{};
    PassThruClose_t PassThruClose{};
    PassThruConnect_t PassThruConnect{};
    PassThruDisconnect_t PassThruDisconnect{};
    PassThruIoctl_t PassThruIoctl{};
    PassThruReadVersion_t PassThruReadVersion{};
    PassThruGetLastError_t PassThruGetLastError{};
    PassThruReadMsgs_t PassThruReadMsgs{};
    PassThruStartMsgFilter_t PassThruStartMsgFilter{};
    PassThruStopMsgFilter_t PassThruStopMsgFilter{};
    PassThruWriteMsgs_t PassThruWriteMsgs{};
    PassThruStartPeriodicMsg_t PassThruStartPeriodicMsg{};
    PassThruStopPeriodicMsg_t PassThruStopPeriodicMsg{};
    PassThruSetProgrammingVoltage_t PassThruSetProgrammingVoltage{};
};

}

#endif // J2534_H
