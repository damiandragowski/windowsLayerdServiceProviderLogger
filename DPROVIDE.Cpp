/*++

     Copyright (c) 1996 Intel Corporation
     Copyright (c) 1996 - 1998 Microsoft Corporation
     All Rights Reserved

     Permission is granted to use, copy and distribute this software and
     its documentation for any purpose and without fee, provided, that
     the above copyright notice and this statement appear in all copies.
     Intel makes no representations about the suitability of this
     software for any purpose.  This software is provided "AS IS."

     Intel specifically disclaims all warranties, express or implied,
     and all liability, including consequential and other indirect
     damages, for the use of this software, including liability for
     infringement of any proprietary rights, and including the
     warranties of merchantability and fitness for a particular purpose.
     Intel does not assume any responsibility for any errors which may
     appear in this software nor any responsibility to update it.


Module Name:

    dprovide.cpp

Abstract:

    This module defines the class dprovider along with its methods.

--*/

#include "precomp.h"
#include <stdlib.h>

GUID TransmitFileGuid = WSAID_TRANSMITFILE;
GUID AcceptExGuid = WSAID_ACCEPTEX;
GUID GetAcceptExSockAddrsGuild = WSAID_GETACCEPTEXSOCKADDRS; 


DPROVIDER::DPROVIDER()
/*++
Routine Description:

    Creates any internal state.

Arguments:

    None

Return Value:

    None

--*/

{
    m_proctable = NULL;
    m_library_handle = NULL;
    m_lib_name = NULL;
    m_acceptex = NULL;
    m_transmitfile = NULL;
}



DPROVIDER::~DPROVIDER()
/*++
Routine Description:

    destroys any internal state.

Arguments:

    None

Return Value:

    None

--*/
{
    int ErrorCode;


    if (m_library_handle)
    {
        if (m_proctable->lpWSPCleanup)
        {
            DEBUGF( DBG_TRACE,
                    ("\nCalling WSPCleanup for provider %X", this));
            //Call the servce provider cleanup routine
            WSPCleanup(&ErrorCode);
        } //if

        // Free the service provider DLL
        FreeLibrary(m_library_handle);
    } //if

    if(m_proctable){
        delete m_proctable;
    }
    delete m_lib_name;
    DEBUGF( DBG_TRACE,
            ("\nDestroying provider %X", this));
}



INT
DPROVIDER::Initialize(
    IN PWCHAR lpszLibFile,
    IN LPWSAPROTOCOL_INFOW lpProtocolInfo
    )
/*++
Routine Description:

    Initializes the DPROVIDER object.

Arguments:

    lpszLibFile - A  Null  terminating  string  that  points to the .DLL of the
                  service to load.

    lpProtocolInfo - A pointer to a WSAPROTOCOL_INFO struct to hand to the
                     provider startup routine.

Return Value:

    If no error occurs, Initialize() returns ERROR_SUCEES.  Otherwise the value
    SOCKET_ERROR  is  returned,  and  a  specific  error  code  is available in
    lpErrno.

--*/
{
    LPWSPSTARTUP        WSPStartupFunc          = NULL;
    WORD                wVersionRequested       = MAKEWORD(2,2);
    INT                 error_code              = 0;
    WSPDATA             WSPData;
    CHAR                LibraryPath[MAX_PATH];
    CHAR                lpszLibFileA[MAX_PATH];

    DEBUGF( DBG_TRACE,
            ("\nInitializing provider %X", this));

    m_proctable = new WSPPROC_TABLE;
    if(!m_proctable){
        DEBUGF(
            DBG_ERR,
            ("\nFailed to allocate WSPPROC_TABLE for provider object"));
        return WSA_NOT_ENOUGH_MEMORY;
    }

    // Zero out contents of m_proctable
    ZeroMemory(
        (PVOID) m_proctable,      // Destination
        sizeof(WSPPROC_TABLE)); // Length


    //
    // Expand the library name to pickup environment/registry variables
    //
    wcstombs (lpszLibFileA, lpszLibFile, MAX_PATH);
    if (!( ExpandEnvironmentStrings(lpszLibFileA,
                                    LibraryPath,
                                    MAX_PATH))){
        DEBUGF(
            DBG_ERR,
            ("\nExpansion of environment variables failed"));
        return WSASYSCALLFAILURE;
    } //if

    
    m_lib_name = new char[strlen(LibraryPath)+1];

    strcpy(m_lib_name,LibraryPath);

    
    //
    // First load the DLL for the service provider. Then get two functions that
    // init the service provider structures.
    //
    m_library_handle = LoadLibrary(LibraryPath);
    if(!m_library_handle){
        DEBUGF(
            DBG_ERR,
            ("\nFailed to load DLL %s",LibraryPath));
        return  WSAEPROVIDERFAILEDINIT;
    }

    WSPStartupFunc = (LPWSPSTARTUP)GetProcAddress(
        m_library_handle,
        "WSPStartup"
        );

    if(!(WSPStartupFunc)){

        DEBUGF( DBG_ERR,("\nCould get startup entry point for %s",
                         lpszLibFile));
        return  WSAEPROVIDERFAILEDINIT;
    }

#if !defined(DEBUG_TRACING)
    error_code = (*WSPStartupFunc)(
        wVersionRequested,
        & WSPData,
        lpProtocolInfo,
        gUpCallTable,
        m_proctable);
#else
    { // declaration block
        LPWSPDATA  pWSPData = & WSPData;
        BOOL       bypassing_call;

        bypassing_call = PREAPINOTIFY((
            DTCODE_WSPStartup,
            & error_code,
            LibraryPath,
            & wVersionRequested,
            & pWSPData,
            & lpProtocolInfo,
            & gUpCallTable,
            & m_proctable));
        if (! bypassing_call) {
            error_code = (*WSPStartupFunc)(
                wVersionRequested,
                & WSPData,
                lpProtocolInfo,
                gUpCallTable,
                m_proctable);
            POSTAPINOTIFY((
                DTCODE_WSPStartup,
                & error_code,
                LibraryPath,
                & wVersionRequested,
                & pWSPData,
                & lpProtocolInfo,
                & gUpCallTable,
                & m_proctable));
        } // if ! bypassing_call
    } // declaration block
#endif // !defined(DEBUG_TRACING)

    if(ERROR_SUCCESS != error_code){
        DEBUGF(DBG_ERR, ("\nWSPStartup for %s Failed",lpszLibFile));
        return error_code;
    }

    //
    // Make sure that all of the procedures at least have a non null pointer.
    //
    if( !m_proctable->lpWSPAccept              ||
        !m_proctable->lpWSPAddressToString     ||
        !m_proctable->lpWSPAsyncSelect         ||
        !m_proctable->lpWSPBind                ||
        !m_proctable->lpWSPCancelBlockingCall  ||
        !m_proctable->lpWSPCleanup             ||
        !m_proctable->lpWSPCloseSocket         ||
        !m_proctable->lpWSPConnect             ||
        !m_proctable->lpWSPDuplicateSocket     ||
        !m_proctable->lpWSPEnumNetworkEvents   ||
        !m_proctable->lpWSPEventSelect         ||
        !m_proctable->lpWSPGetOverlappedResult ||
        !m_proctable->lpWSPGetPeerName         ||
        !m_proctable->lpWSPGetSockName         ||
        !m_proctable->lpWSPGetSockOpt          ||
        !m_proctable->lpWSPGetQOSByName        ||
        !m_proctable->lpWSPIoctl               ||
        !m_proctable->lpWSPJoinLeaf            ||
        !m_proctable->lpWSPListen              ||
        !m_proctable->lpWSPRecv                ||
        !m_proctable->lpWSPRecvDisconnect      ||
        !m_proctable->lpWSPRecvFrom            ||
        !m_proctable->lpWSPSelect              ||
        !m_proctable->lpWSPSend                ||
        !m_proctable->lpWSPSendDisconnect      ||
        !m_proctable->lpWSPSendTo              ||
        !m_proctable->lpWSPSetSockOpt          ||
        !m_proctable->lpWSPShutdown            ||
        !m_proctable->lpWSPSocket              ||
        !m_proctable->lpWSPStringToAddress ){

        DEBUGF(DBG_ERR,
               ("\nService provider %s returned an invalid procedure table",
                lpszLibFile));
        return WSAEINVALIDPROCTABLE;
    }

    //
    // Confirm that the WinSock service provider supports 2.0. If it supports a
    // version greater then 2.0 it will still return 2.0 since this is the
    // version  we requested.
    //
    if(WSPData.wVersion != MAKEWORD(2,2)){
        if(m_proctable->lpWSPCleanup) {
            if(m_proctable->lpWSPCleanup(&error_code)){
                DEBUGF( DBG_ERR,
                        ("\nService Provider %s does not support version 2.2",
                         lpszLibFile));
                return WSAVERNOTSUPPORTED;
            }
        }
        return WSAEINVALIDPROVIDER;
    }

    return ERROR_SUCCESS;
} //Initailize


INT
DPROVIDER::InterceptExtensions (
    IN LPVOID       lpvInBuffer,
    OUT LPVOID      lpvOutBuffer,
    OUT LPINT       lpErrno
    )
{
    INT ReturnValue=NO_ERROR;

    if (memcmp (lpvInBuffer,
                &TransmitFileGuid,
                sizeof (GUID))==0) {
        m_transmitfile = *((LPFN_TRANSMITFILE *)lpvOutBuffer);
        *((LPFN_TRANSMITFILE *)lpvOutBuffer) = WSPTransmitFile;
    }
    else if (memcmp (lpvInBuffer,
                &AcceptExGuid,
                sizeof (GUID))==0) {
        m_acceptex = *((LPFN_ACCEPTEX *)lpvOutBuffer);
        *((LPFN_ACCEPTEX *)lpvOutBuffer) = WSPAcceptEx;
    }
    else if (memcmp (lpvInBuffer,
                &GetAcceptExSockAddrsGuild,
                sizeof (GUID))==0)
        // No translation required because no socket handle is
        // involved in this call
        ;
    else {
        ReturnValue = SOCKET_ERROR;
        *lpErrno = WSAEOPNOTSUPP;
    }

    return ReturnValue;
}

