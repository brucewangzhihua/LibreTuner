#ifndef ISOTP_H
#define ISOTP_H

#include "network/can/can.h"

#include <cstdint>
#include <chrono>
#include <vector>

namespace lt {
namespace network {

    
struct IsoTpOptions {
    uint32_t sourceId = 0x7E0, destId = 0x7E8;
    std::chrono::milliseconds timeout{10000};  
};



class IsoTpPacket {
public:
    IsoTpPacket();
    IsoTpPacket(const uint8_t *data, size_t size);

    /* Resets packet data to `data` */
    void setData(const uint8_t *data, size_t size);

    /* moves the data into the vector */
    void moveInto(std::vector<uint8_t> &data);

    /* Appends data to the end of the packet */
    void append(const uint8_t *data, size_t size);

    std::vector<uint8_t>::size_type size() const { return data_.size(); }

    uint8_t &operator[](int index) { return data_[index]; }

    uint8_t operator[](int index) const { return data_[index]; }

    void clear() {
        data_.clear();
    }

	std::vector<uint8_t>::iterator begin() { return data_.begin(); }
    std::vector<uint8_t>::const_iterator cbegin() const {
        return data_.cbegin(); }

    std::vector<uint8_t>::iterator end() { return data_.end(); }
    std::vector<uint8_t>::const_iterator cend() const { return data_.cend(); }

	uint8_t *data() { return data_.data(); }
    const uint8_t *data() const { return data_.data(); }

private:
    std::vector<uint8_t> data_;
};

class IsoTpPacketReader {
public:
	IsoTpPacketReader(const IsoTpPacket &packet) : packet_(packet) {};

	std::size_t remaining() const { return packet_.size()  - pointer_; }

	// Returns the next bytes in the packet, stopping at `max` bytes
	std::vector<uint8_t> next(std::size_t max);
	// Returns the amount of bytes read
	std::size_t next(uint8_t *dest, std::size_t max);

	std::vector<uint8_t> readRemaining();

private:
	const IsoTpPacket &packet_;
	std::size_t pointer_{ 0 };
};

// ISO 15765-2 transport layer (ISO-TP) for sending large packets over CAN
class IsoTp {
public:
    // Takes ownership of a CAN interface
    IsoTp(CanPtr &&can = CanPtr(), IsoTpOptions options = IsoTpOptions());
    ~IsoTp();

    void recv(IsoTpPacket &result);

    // Sends a request and waits for a response
    void request(const IsoTpPacket &req, IsoTpPacket &result);

    void send(const IsoTpPacket &packet);

    void setCan(CanPtr &&can) { can_ = std::move(can); }

    // May return nullptr
    Can *can() { return can_.get(); }

    void setOptions(const IsoTpOptions &options) { options_ = options; }

    const IsoTpOptions &options() const { return options_; }

	// Receives next CAN message with proper id
	CanMessage recvNextFrame();
	CanMessage recvNextFrame(uint8_t expectedType);

private:
    CanPtr can_;
    IsoTpOptions options_;

	void sendSingleFrame(const uint8_t *data, std::size_t size);
};

} // namespace network
} // namespace lt

#endif // ISOTP_H
