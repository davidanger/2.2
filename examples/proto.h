/* sys
 */

#if !defined(_PROTO_)
#define _PROTO_

#include <stdint.h>

/* This is our own protocol
 */
struct header {
    uint32_t   opCode;
    uint32_t   len;
};

/* This is how we encapsulate
 * SOCKS5 protocol specification.
 *
 * The initial greeting from the client is

 * field 1: SOCKS version number (must be 0x05 for this version)
 * field 2: number of authentication methods supported, 1 byte
 * field 3: authentication methods, variable length,
 * 1 byte per method supported
 *
 * The server's choice is communicated:
 *
 * field 1: SOCKS version, 1 byte (0x05 for this version)
 * field 2: chosen authentication method, 1 byte,
 * or 0xFF if no acceptable methods were offered
 *
 */
struct greetReq {
    uint8_t   version;
    uint8_t   authnum;
    uint8_t   auth;
};

struct greetReply {
    uint8_t   version;
    uint8_t   auth;
};

/* The client's connection request 10bytes is
 *
 * field 1: SOCKS version number, 1 byte (must be 0x05 for this version)
 * field 2: command code, 1 byte:
 *    o 0x01 = establish a TCP/IP stream connection
 *    o 0x02 = establish a TCP/IP port binding
 *    o 0x03 = associate a UDP port
 * field 3: reserved, must be 0x00
 * field 4: address type, 1 byte:
 *    o 0x01 = IPv4 address
 *    o 0x03 = Domain name
 *    o 0x04 = IPv6 address
 * field 5: destination address of
 *    o 4 bytes for IPv4 address
 *    o 1 byte of name length followed by the name for Domain name
 *    o 16 bytes for IPv6 address
 * field 6: port number in a network byte order, 2 bytes
 *
 */

struct connRequest {
    uint8_t    version;
    uint8_t    command;
    uint8_t    reserved;
    uint8_t    addrtype;
    uint32_t   addr;      /* later on we unionize this */
    uint16_t   port;
};

/* Server response 10bytes:
 *
 * field 1: SOCKS protocol version, 1 byte (0x05 for this version)
 * field 2: status, 1 byte:
 *   o 0x00 = request granted
 *   o 0x01 = general failure
 *   o 0x02 = connection not allowed by ruleset
 *   o 0x03 = network unreachable
 *   o 0x04 = host unreachable
 *   o 0x05 = connection refused by destination host
 *   o 0x06 = TTL expired
 *   o 0x07 = command not supported / protocol error
 *   o 0x08 = address type not supported
 * field 3: reserved, must be 0x00
 * field 4: address type, 1 byte:
 *   o 0x01 = IPv4 address
 *   o 0x03 = Domain name
 *   o 0x04 = IPv6 address
 * field 5: destination address of
 *   o 4 bytes for IPv4 address
 *   o 1 byte of name length followed by the name for Domain name
 *   o 16 bytes for IPv6 address
 * field 6: network byte order port number, 2 bytes
 */
struct connReply {
    uint8_t    version;
    uint8_t    status;
    uint8_t    reserved;
    uint8_t    addrtype;
    uint32_t   addr;
    uint16_t   port;
};

#endif /* _PROTO_ */
