////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `ionik-lib`.
//
// Changelog:
//      2023.02.16 Initial version (netty-lib).
//      2024.04.03 Moved from `netty-lib`.
////////////////////////////////////////////////////////////////////////////////
#include "net/netlink_socket.hpp"
#include "pfs/i18n.hpp"
#include <linux/rtnetlink.h>

#if IONIK__LIBMNL_ENABLED
#   include <libmnl/libmnl.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <unistd.h>
#endif

namespace ionik {
namespace net {

netlink_socket::netlink_socket () = default;

netlink_socket::netlink_socket (type_enum netlinktype)
{
    switch (netlinktype) {
        case type_enum::route: {
#if IONIK__LIBMNL_ENABLED
            _socket = mnl_socket_open(NETLINK_ROUTE);
#else
            _socket = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
#endif
            break;
        }
        default: {
            throw error {
                  errc::socket_error
                , tr::f_("bad/unsupported Netlink socket type: {}"
                    , static_cast<int>(netlinktype))
            };
        }
    }

#if IONIK__LIBMNL_ENABLED
    if (_socket == nullptr) {
#else
    if (_socket < 0) {
#endif
        throw error {
              errc::socket_error
            , tr::_("create Netlink socket failure")
            , pfs::system_error_text()
        };
    }

    unsigned int nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR
        /* | RTMGRP_NOTIFY | RTMGRP_IPV6_IFADDR */;

#if IONIK__LIBMNL_ENABLED
    auto rc = mnl_socket_bind(_socket, nl_groups, MNL_SOCKET_AUTOPID);
#else
    sockaddr_nl addr_nl;

    std::memset(& addr_nl, 0, sizeof(addr_nl));

    addr_nl.nl_family = AF_NETLINK;
    addr_nl.nl_pad = 0;
    addr_nl.nl_pid = 0; // getpid();
    addr_nl.nl_groups = nl_groups;

    auto rc = ::bind(_socket, reinterpret_cast<sockaddr *>(& addr_nl), sizeof(addr_nl));
#endif

    if (rc < 0) {
        throw error {
              errc::socket_error
            , tr::_("bind Netlink socket failure")
            , pfs::system_error_text()
        };
    }
}

netlink_socket::~netlink_socket ()
{
#if IONIK__LIBMNL_ENABLED
    if (_socket != nullptr)
        mnl_socket_close(_socket);

    _socket = nullptr;
#else
    if (_socket > 0)
        ::close(_socket);

    _socket = kINVALID_SOCKET;
#endif
}

netlink_socket::netlink_socket (netlink_socket && other)
{
    this->operator = (std::move(other));
}

netlink_socket & netlink_socket::operator = (netlink_socket && other)
{
    this->~netlink_socket();
    _socket = other._socket;

#if IONIK__LIBMNL_ENABLED
    other._socket = nullptr;
#else
    other._socket = kINVALID_SOCKET;
#endif

    return *this;
}

netlink_socket::operator bool () const noexcept
{
#if IONIK__LIBMNL_ENABLED
    return _socket != nullptr;
#else
    return _socket > 0;
#endif
}

netlink_socket::native_type netlink_socket::native () const noexcept
{
#if IONIK__LIBMNL_ENABLED
    if (_socket == nullptr)
        return kINVALID_SOCKET;

    return mnl_socket_get_fd(_socket);
#else
    return _socket;
#endif
}

int netlink_socket::recv (char * data, int len, error * perr)
{
#if IONIK__LIBMNL_ENABLED
    auto n = mnl_socket_recvfrom(_socket, data, len);
#else

    sockaddr_nl addr_nl;
    iovec iov;

    std::memset(& iov, 0, sizeof(iov));

    iov.iov_base = data;
    iov.iov_len  = len;

    msghdr m;

    std::memset(& m, 0, sizeof(m));

    m.msg_name       = & addr_nl;
    m.msg_namelen    = sizeof(sockaddr_nl);
    m.msg_iov        = & iov;
    m.msg_iovlen     = 1;
    m.msg_control    = nullptr;
    m.msg_controllen = 0;
    m.msg_flags      = 0;

    auto n = ::recvmsg(_socket, & m, 0);

    if (n < 0) {
        ;
    } else if (m.msg_flags & MSG_TRUNC) {
        errno = ENOSPC;
        n = -1;
    } else if (m.msg_namelen != sizeof(sockaddr_nl)) {
        errno = EINVAL;
        n = -1;
    }
#endif

    if (n < 0) {
        error err {
              errc::socket_error
            , tr::_("receive data from Netlink socket failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
                return n;
        } else {
            throw err;
        }
    }

    return n;
}

int netlink_socket::send (char const * req, int len, error * perr)
{
#if IONIK__LIBMNL_ENABLED
    auto n = mnl_socket_sendto(_socket, req, len);
#else
    sockaddr_nl addr_nl;

    std::memset(& addr_nl, 0, sizeof(addr_nl));

    addr_nl.nl_family = AF_NETLINK;

    auto n = ::sendto(_socket, req, len, 0
        , reinterpret_cast<sockaddr *>(& addr_nl), sizeof(addr_nl));
#endif

    if (n < 0) {
        error err {
              errc::socket_error
            , tr::_("send Netlink request failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
                return n;
        } else {
            throw err;
        }
    }

    return n;
}

}} // namespace ionik::net