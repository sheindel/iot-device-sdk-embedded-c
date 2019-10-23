/* Copyright 2018-2019 Google LLC
 *
 * This is part of the Google Cloud IoT Device SDK for Embedded C.
 * It is licensed under the BSD 3-Clause license; you may not use this file
 * except in compliance with the License.
 *
 * You may obtain a copy of the License at:
 *  https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <simplelink.h>
#include <iotc_bsp_io_net.h>
#include <stdio.h>
#include <string.h>
#include <iotc_macros.h>
#include <iotc_debug.h>
#include <iotc_bsp_debug.h> // _bsp_debug_logger
#include "iotc_bsp_hton.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif


iotc_bsp_io_net_state_t 
iotc_bsp_io_net_create_socket( iotc_bsp_socket_t* iotc_socket )
{
    if ( NULL == iotc_socket )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    int retval = 0;

#ifdef IOTC_BSP_IO_NET_TLS_SOCKET
    unsigned long ulToken;

    if ( ( retval = CheckOrInstallCertficiateOnDevice( &ulToken ) ) < 0 )
    {
        iotc_debug_printf( "ERROR: CheckOrInstallCertficiateOnDevice result: %d\n\r",
                         retval );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    {
        /* set current time for certificate validity check during TLS handshake */
        const time_t current_time_seconds = iotc_bsp_time_getcurrenttime_seconds();
        struct tm* const gm_time          = localtime( &current_time_seconds );

        /* conversion between tm datetime representation to Sl datetime representation
           tm years: since 1970, Sl years: since 0
           tm months: 0-11, Sl months: 1-12 */

        SlDateTime_t sl_datetime;
        sl_datetime.tm_year = gm_time->tm_year + 1970;
        sl_datetime.tm_mon  = gm_time->tm_mon + 1;
        sl_datetime.tm_day  = gm_time->tm_mday;
        sl_datetime.tm_hour = gm_time->tm_hour;
        sl_datetime.tm_min  = gm_time->tm_min;
        sl_datetime.tm_sec  = gm_time->tm_sec;

        retval =
            sl_DeviceSet( SL_DEVICE_GENERAL, SL_DEVICE_GENERAL_DATE_TIME,
                          sizeof( SlDateTime_t ), ( unsigned char* )( &sl_datetime ) );

        if ( retval < 0 )
        {
            return IOTC_BSP_IO_NET_STATE_ERROR;
        }
    }

    /* open a secure socket */
    *iotc_socket = sl_Socket( SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET );

    if ( *iotc_socket < 0 )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

#ifdef IOTC_CC3220SF_UNSAFELY_DISABLE_CERT_STORE
    /* Disable usage of the on-board Certificate Catalog - Also disables the CRL */
    int32_t dummyValue = 0;
    retval =
        sl_SetSockOpt( *iotc_socket, SL_SOL_SOCKET, SL_SO_SECURE_DISABLE_CERTIFICATE_STORE,
                       &dummyValue, sizeof( dummyValue ) );
    if ( retval < 0 )
    {
        iotc_bsp_debug_logger(
            "[ERROR] Failed to disable certificate catalog validation\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }
#endif /* IOTC_CC3220SF_UNSAFELY_DISABLE_CERT_STORE */

    /* set TLS version to 1.2 */
    unsigned char ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
    retval = sl_SetSockOpt( *iotc_socket, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,
                            sizeof( ucMethod ) );
    if ( retval < 0 )
    {
        iotc_bsp_debug_logger( "[ERROR] Failed to set TLSv1.2 socket option\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    /* set trusted Root CA Cert file path */
    retval = sl_SetSockOpt( *iotc_socket, SL_SOL_SOCKET, SL_SO_SECURE_FILES_CA_FILE_NAME,
                            IOTC_CC32XX_ROOTCACERT_FILE_NAME,
                            strlen( IOTC_CC32XX_ROOTCACERT_FILE_NAME ) );
    if ( retval < 0 )
    {
        iotc_bsp_debug_logger( "[ERROR] Failed to set root certificate\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

#else
    /* open a socket */
    *iotc_socket = sl_Socket( SL_AF_INET, SL_SOCK_STREAM, 0 );

    if ( *iotc_socket < 0 )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

#endif /* IOTC_BSP_IO_NET_TLS_SOCKET */

    int sl_nonblockingOption = 1;
    retval                   = sl_SetSockOpt( *iotc_socket, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
                            &sl_nonblockingOption, sizeof( sl_nonblockingOption ) );

    if ( retval < 0 )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    return IOTC_BSP_IO_NET_STATE_OK;
}

iotc_bsp_io_net_state_t
iotc_bsp_io_net_connect( iotc_bsp_socket_t* iotc_socket, const char* host, uint16_t port )
{
    if ( NULL == iotc_socket || NULL == host )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    unsigned long uiIP;
    int errval = 0;

#ifdef IOTC_BSP_IO_NET_TLS_SOCKET
    /* Set hostname for verification by the on-board TLS implementation */
    errval = sl_SetSockOpt( *iotc_socket, SL_SOL_SOCKET,
                            SL_SO_SECURE_DOMAIN_NAME_VERIFICATION, host, strlen( host ) );
    if ( errval < 0 )
    {
        iotc_bsp_debug_logger( "[ERROR] Failed to configure domain name validation\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }
#endif

    /* Resolve hostname to IP address */
    errval = sl_NetAppDnsGetHostByName( ( _i8* )host, strlen( host ), &uiIP, SL_AF_INET );

    if ( 0 != errval )
    {
        iotc_debug_printf( "Error: couldn't resolve hostname\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    SlSockAddrIn_t name = {.sin_family      = SL_AF_INET, /* SL_AF_INET */
                               .sin_port        = htons( port ),
                               .sin_addr.s_addr = htonl( uiIP ),
                               .sin_zero        = {0}};

    errval =
        sl_Connect( *iotc_socket, ( SlSockAddr_t* )&name, sizeof( SlSockAddr_t ) );

    if ( SL_ERROR_BSD_ESECUNKNOWNROOTCA == errval )
    {
        iotc_bsp_debug_logger( "[SECURITY CHECK FAILED] Unknown Root CA\n\r" );
        iotc_bsp_debug_logger( "\tAborting connection to the MQTT broker\n\r" );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }
    else if ( errval < 0 && errval != SL_ERROR_BSD_EALREADY )
    {
        iotc_debug_printf( "Error on sl_Connect: %d\n\r", errval );
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    return IOTC_BSP_IO_NET_STATE_OK;
}

iotc_bsp_io_net_state_t iotc_bsp_io_net_socket_connect(
    iotc_bsp_socket_t* iotc_socket, const char* host, uint16_t port,
    iotc_bsp_socket_type_t socket_type) {

    iotc_bsp_io_net_state_t bsp_state = IOTC_BSP_IO_NET_STATE_OK;

 // GN: e.g. ... from _io_net_layer.c

  bsp_state = iotc_bsp_io_net_create_socket(iotc_socket);

  if (IOTC_BSP_IO_NET_STATE_OK == bsp_state) {
    iotc_debug_logger("Socket creation [ok]");
  }
  else {
    iotc_debug_logger("Socket creation [failed]");
    return bsp_state;
  }

  bsp_state = iotc_bsp_io_net_connect(iotc_socket, host, port);

  if (IOTC_BSP_IO_NET_STATE_OK == bsp_state) {
    iotc_debug_logger("Socket connect() [ok]");
  }

  return bsp_state; // IOTC_BSP_IO_NET_STATE_OK;
}


iotc_bsp_io_net_state_t iotc_bsp_io_net_connection_check(
    iotc_bsp_socket_t iotc_socket, const char* host, uint16_t port) {

    return iotc_bsp_io_net_connect( &iotc_socket, host, port );
}

iotc_bsp_io_net_state_t iotc_bsp_io_net_write(iotc_bsp_socket_t iotc_socket,
                                              int* out_written_count,
                                              const uint8_t* buf,
                                              size_t count) {

    if ( NULL == out_written_count || NULL == buf )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    *out_written_count = sl_Send( iotc_socket, buf, count, 0 );

    /* TI's SimpleLink write() returns errors in the return value */
    if ( SL_ERROR_BSD_EAGAIN == *out_written_count )
    {
        return IOTC_BSP_IO_NET_STATE_BUSY;
    }
    else if ( *out_written_count < 0 )
    {
        *out_written_count = 0;
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    return IOTC_BSP_IO_NET_STATE_OK;
}

iotc_bsp_io_net_state_t iotc_bsp_io_net_read(iotc_bsp_socket_t iotc_socket,
                                             int* out_read_count, uint8_t* buf,
                                             size_t count) {
    if ( NULL == out_read_count || NULL == buf )
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }

    *out_read_count = sl_Recv( iotc_socket, buf, count, 0 );

    if ( SL_ERROR_BSD_EAGAIN == *out_read_count )
    {
        return IOTC_BSP_IO_NET_STATE_BUSY;
    }
    else if ( *out_read_count < 0 )
    {
        *out_read_count = 0;
        return IOTC_BSP_IO_NET_STATE_ERROR;
    }
    else if ( 0 == *out_read_count )
    {
        return IOTC_BSP_IO_NET_STATE_CONNECTION_RESET;
    }

    return IOTC_BSP_IO_NET_STATE_OK;
}

iotc_bsp_io_net_state_t iotc_bsp_io_net_close_socket(
    iotc_bsp_socket_t* iotc_socket) {

    if ((NULL == iotc_socket)
        || (0 > iotc_socket)
        || (0 > *iotc_socket))
    {
        return IOTC_BSP_IO_NET_STATE_ERROR;    
    }
    
    sl_Close( *iotc_socket );

    return IOTC_BSP_IO_NET_STATE_OK;
}

iotc_bsp_io_net_state_t iotc_bsp_io_net_select(
    iotc_bsp_socket_events_t* socket_events_array,
    size_t socket_events_array_size, long timeout_sec) {

    SlFdSet_t rfds;
    SlFdSet_t wfds;
    SlFdSet_t efds;

    SL_SOCKET_FD_ZERO( &rfds );
    SL_SOCKET_FD_ZERO( &wfds );
    SL_SOCKET_FD_ZERO( &efds );

    int max_fd_read  = 0;
    int max_fd_write = 0;
    int max_fd_error = 0;

    SlTimeval_t tv = {0, 0};

    /* translate the library socket events settings to the event sets used by posix
       select */
    size_t socket_id = 0;
    for ( socket_id = 0; socket_id < socket_events_array_size; ++socket_id )
    {
        iotc_bsp_socket_events_t* socket_events = &socket_events_array[socket_id];

        if ( NULL == socket_events )
        {
            return IOTC_BSP_IO_NET_STATE_ERROR;
        }

        if ( 1 == socket_events->in_socket_want_read )
        {
            SL_SOCKET_FD_SET( socket_events->iotc_socket, &rfds );
            max_fd_read = socket_events->iotc_socket > max_fd_read
                              ? socket_events->iotc_socket
                              : max_fd_read;
        }

        if ( ( 1 == socket_events->in_socket_want_write ) ||
             ( 1 == socket_events->in_socket_want_connect ) )
        {
            SL_SOCKET_FD_SET( socket_events->iotc_socket, &wfds );
            max_fd_write = socket_events->iotc_socket > max_fd_write
                               ? socket_events->iotc_socket
                               : max_fd_write;
        }

        if ( 1 == socket_events->in_socket_want_error )
        {
            SL_SOCKET_FD_SET( socket_events->iotc_socket, &efds );
            max_fd_error = socket_events->iotc_socket > max_fd_error
                               ? socket_events->iotc_socket
                               : max_fd_error;
        }
    }

    /* calculate max fd */
    const int max_fd = MAX( max_fd_read, MAX( max_fd_write, max_fd_error ) );

    tv.tv_sec = 1;

    /* call the actual posix select */
    const int result = sl_Select( max_fd + 1, &rfds, &wfds, &efds, &tv );

    if ( 0 < result )
    {
        /* translate the result back to the socket events structure */
        for ( socket_id = 0; socket_id < socket_events_array_size; ++socket_id )
        {
            iotc_bsp_socket_events_t* socket_events = &socket_events_array[socket_id];

            if ( SL_SOCKET_FD_ISSET( socket_events->iotc_socket, &rfds ) )
            {
                socket_events->out_socket_can_read = 1;
            }

            if ( SL_SOCKET_FD_ISSET( socket_events->iotc_socket, &wfds ) )
            {
                if ( 1 == socket_events->in_socket_want_connect )
                {
                    socket_events->out_socket_connect_finished = 1;
                }

                if ( 1 == socket_events->in_socket_want_write )
                {
                    socket_events->out_socket_can_write = 1;
                }
            }

            if ( SL_SOCKET_FD_ISSET( socket_events->iotc_socket, &efds ) )
            {
                socket_events->out_socket_error = 1;
            }
        }

        return IOTC_BSP_IO_NET_STATE_OK;
    }
    else if ( 0 == result )
    {
        return IOTC_BSP_IO_NET_STATE_OK;
    }

    return IOTC_BSP_IO_NET_STATE_ERROR;
}

#ifdef __cplusplus
}
#endif

