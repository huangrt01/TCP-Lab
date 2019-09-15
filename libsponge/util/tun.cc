#include "tun.hh"

#include "util.hh"

#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

static constexpr const char *CLONEDEV = "/dev/net/tun";

using namespace std;

//! \param[in] devname is the name of the TUN device, specified at its creation.
//!
//! To create a TUN device, you should already have run
//!
//!     ip tuntap add mode tun user `username` name `devname`
//!
//! as root before calling this function.

TunFD::TunFD(const string &devname) : FileDescriptor(SystemCall("open", open(CLONEDEV, O_RDWR))) {
    struct ifreq tun_req {};

    tun_req.ifr_flags = IFF_TUN | IFF_NO_PI;  // tun device with no packetinfo

    // copy devname to ifr_name, making sure to null terminate

    strncpy(static_cast<char *>(tun_req.ifr_name), devname.data(), IFNAMSIZ - 1);
    tun_req.ifr_name[IFNAMSIZ - 1] = '\0';

    SystemCall("ioctl", ioctl(fd_num(), TUNSETIFF, static_cast<void *>(&tun_req)));
}
